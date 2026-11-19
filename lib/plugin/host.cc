#ifdef TINY_HAVE_LUA

#include "plugin/host.h"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <utility>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include "plugin/json.h"
#include "plugin/widgets.h"
#include "runner.h"
#include "spawn.h"
#include "text.h"

#if LUA_VERSION_NUM < 503
#error "tiny-shell needs Lua 5.3 or newer"
#endif

namespace tiny::plugin {

namespace {

using namespace std::chrono_literals;

constexpr const char* widget_meta = "tiny.widget";
constexpr const char* timer_meta = "tiny.timer";
//! A function that runs longer than this is stopped
constexpr auto time_limit = 2s;
constexpr int hook_every = 100000; //!< VM instructions between the checks
constexpr int shortest_timer_ms = 10;
//! The longest delay of a timer or a program (30 days): a longer one, or one
//! that is not a number, would overflow the clock
constexpr long long longest_delay_ms = 30LL * 24 * 3600 * 1000;
constexpr size_t default_file_limit = size_t{1024} * 1024;

struct Plugin;

//! What the plugins share: the services and the configuration
struct HostState {
    HostServices services;
    wm::Config config;
    std::vector<std::unique_ptr<Plugin>> running;
    std::map<std::string, std::string> errors;
    //! What was asked to run at the last `apply()`, to see what changed
    struct Attempt {
        std::string id;
        std::string dir;
        std::string version;
        std::map<std::string, std::string> values;
    };
    std::vector<Attempt> attempts;
    bool told_no_panel = false;
};

//! A widget of a plugin in the panel
struct Entry {
    std::unique_ptr<tiny::Widget> widget;
    PluginLabel* label = nullptr;   //!< the widget when it is a label
    PluginButton* button = nullptr; //!< the widget when it is a button
    std::unique_ptr<PluginPopup> popup; //!< made at the first `popup()`
    int click_ref = LUA_NOREF;
};

struct Plugin {
    HostState* host = nullptr;
    PluginInfo info;
    lua_State* L = nullptr;
    std::unique_ptr<Runner> runner;
    std::vector<std::unique_ptr<Entry>> entries;
    //! Widgets that were destroyed from inside of their own handler
    std::vector<std::unique_ptr<tiny::Widget>> graveyard;
    //! The timers: the id and the reference to the function in the registry
    std::map<Timers::Id, int> timers;
    int config_ref = LUA_NOREF;                //!< `tiny.on_config`
    std::map<std::string, std::string> values; //!< the settings at the start
    int depth = 0; //!< the calls of Lua that are in progress
    std::chrono::steady_clock::time_point call_start;
    std::set<std::string> loading; //!< `require` in progress
};

#ifdef __linux__
constexpr const char* platform_name = "linux";
#elif defined(__FreeBSD__)
constexpr const char* platform_name = "freebsd";
#elif defined(__OpenBSD__)
constexpr const char* platform_name = "openbsd";
#elif defined(__NetBSD__)
constexpr const char* platform_name = "netbsd";
#elif defined(__DragonFly__)
constexpr const char* platform_name = "dragonfly";
#elif defined(__APPLE__)
constexpr const char* platform_name = "macos";
#else
constexpr const char* platform_name = "unknown";
#endif

const char plugin_key = 0; //!< the address is the key of the registry

Plugin& self(lua_State* L)
{
    lua_rawgetp(L, LUA_REGISTRYINDEX, &plugin_key);
    auto* plugin = static_cast<Plugin*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return *plugin;
}

void log_line(HostState& host, const std::string& line)
{
    if (host.services.log) {
        host.services.log(line);
    } else {
        fprintf(stderr, "%s\n", line.c_str());
    }
}

void log_plugin(const Plugin& plugin, const std::string& text)
{
    log_line(*plugin.host,
             "tiny-shell: plugin " + plugin.info.id + ": " + text);
}

// ---- calling Lua ----

void time_hook(lua_State* L, lua_Debug*)
{
    const Plugin& plugin = self(L);
    if (std::chrono::steady_clock::now() - plugin.call_start > time_limit) {
        // From now on every instruction raises the error, so a `pcall` around
        // a loop does not swallow it for good (`call()` sets the pace again)
        lua_sethook(L, time_hook, LUA_MASKCOUNT, 1);
        luaL_error(L, "the plugin runs for too long (more than %d seconds)",
                   static_cast<int>(std::chrono::seconds(time_limit).count()));
    }
}

int traceback(lua_State* L)
{
    const char* message = lua_tostring(L, 1);
    if (message == nullptr) { // an error that is not a text
        message = "(an error that is not a text)";
    }
    luaL_traceback(L, L, message, 1);
    return 1;
}

//! Call the function under `nargs` arguments on the stack. False and the
//! error in the log when it fails; the results (`nresults`) stay on the stack.
bool call(Plugin& plugin, int nargs, int nresults)
{
    lua_State* L = plugin.L;
    const int base = lua_gettop(L) - nargs;
    lua_pushcfunction(L, traceback);
    lua_insert(L, base);
    if (plugin.depth++ == 0) {
        plugin.call_start = std::chrono::steady_clock::now();
        lua_sethook(L, time_hook, LUA_MASKCOUNT, hook_every);
    }
    const int status = lua_pcall(L, nargs, nresults, base);
    --plugin.depth;
    lua_remove(L, base);
    if (status != LUA_OK) {
        const char* message = lua_tostring(L, -1);
        log_plugin(plugin, message != nullptr ? message : "an error");
        lua_pop(L, 1);
        return false;
    }
    return true;
}

//! Call the function that is in the registry
bool call_ref(Plugin& plugin, int ref, int nargs_pushed = 0)
{
    // the arguments are already on the stack: put the function under them
    lua_State* L = plugin.L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    if (nargs_pushed > 0) {
        lua_insert(L, lua_gettop(L) - nargs_pushed);
    }
    return call(plugin, nargs_pushed, 0);
}

// ---- helpers of the functions ----

// Raising an error in Lua jumps over the C++ objects that are alive (Lua is C),
// so the functions below that make objects (strings, vectors) never raise
// one: they push the message and return false (or -1 from a function of Lua),
// and the registered function, where nothing is alive, raises it.

//! Push an error and return -1, the result of a function that failed
int fail(lua_State* L, const char* message)
{
    lua_pushstring(L, message);
    return -1;
}

//! Raise the error that `fail()` pushed, or return the number of results
int finish(lua_State* L, int result)
{
    return result < 0 ? lua_error(L) : result;
}

//! The strings of the table at `index`, for a program to run; false and the
//! error pushed when the table is not right
bool string_list(lua_State* L, int index, std::vector<std::string>& list)
{
    if (!lua_istable(L, index)) {
        fail(L, "the program and its arguments are a table of strings");
        return false;
    }
    const auto size = luaL_len(L, index);
    for (lua_Integer i = 1; i <= size; ++i) {
        lua_rawgeti(L, index, i);
        if (lua_type(L, -1) != LUA_TSTRING) {
            lua_pop(L, 1);
            fail(L, "the program and its arguments are strings");
            return false;
        }
        size_t length = 0;
        const char* text = lua_tolstring(L, -1, &length);
        list.emplace_back(text, length);
        lua_pop(L, 1);
    }
    if (list.empty() || list.front().empty()) {
        fail(L, "the name of the program is missing");
        return false;
    }
    return true;
}

//! The text field `name` of the table at `index` goes to `value` (it is not
//! changed when there is none); false and the error pushed for another type
bool field_string(lua_State* L, int index, const char* name, std::string& value)
{
    lua_getfield(L, index, name);
    bool good = true;
    if (lua_type(L, -1) == LUA_TSTRING || lua_type(L, -1) == LUA_TNUMBER) {
        value = lua_tostring(L, -1); // a number is converted in a copy
    } else if (!lua_isnil(L, -1)) {
        good = false;
    }
    lua_pop(L, 1);
    if (!good) {
        lua_pushfstring(L, "%s must be a text", name);
    }
    return good;
}

//! A delay in milliseconds that a plugin asked for, from `lowest` to
//! `longest_delay_ms`; what is not a number is the lowest
std::chrono::milliseconds delay_of(lua_Number value, long long lowest)
{
    if (!(value >= static_cast<lua_Number>(lowest))) { // also NaN
        return std::chrono::milliseconds(lowest);
    }
    if (value > static_cast<lua_Number>(longest_delay_ms)) {
        return std::chrono::milliseconds(longest_delay_ms);
    }
    return std::chrono::milliseconds(static_cast<long long>(value));
}

// ---- tiny.log, tiny.setting, tiny.config, tiny.theme ----

int l_log(lua_State* L)
{
    std::string line;
    const int count = lua_gettop(L);
    for (int i = 1; i <= count; ++i) {
        size_t length = 0;
        const char* text = luaL_tolstring(L, i, &length);
        line += (i > 1 ? " " : "") + std::string(text, length);
        lua_pop(L, 1);
    }
    log_plugin(self(L), line);
    return 0;
}

int l_setting(lua_State* L)
{
    const Plugin& plugin = self(L);
    const std::string key = luaL_checkstring(L, 1);
    const SettingSpec* spec = plugin.info.find_setting(key);
    if (spec == nullptr) {
        lua_pushnil(L); // it is not in plugin.ini
        return 1;
    }
    const std::string value =
            setting_value(*spec, plugin.host->config, plugin.info.id);
    switch (spec->type) {
        case SettingType::Toggle:
            lua_pushboolean(L, value == "yes");
            break;
        case SettingType::Number: {
            const double number = strtod(value.c_str(), nullptr);
            if (number == std::floor(number) && std::fabs(number) < 1e15) {
                lua_pushinteger(L, static_cast<lua_Integer>(number));
            } else {
                lua_pushnumber(L, number);
            }
            break;
        }
        case SettingType::Choice:
        case SettingType::Text:
            lua_pushlstring(L, value.data(), value.size());
            break;
    }
    return 1;
}

int l_config(lua_State* L)
{
    luaL_checkstring(L, 1); // both before any object is made
    luaL_checkstring(L, 2);
    const std::string section = lua_tostring(L, 1);
    const std::string key = lua_tostring(L, 2);
    const auto& raw = self(L).host->config.raw;
    const auto it = raw.find({section, key});
    if (it == raw.end()) {
        lua_pushnil(L);
    } else {
        lua_pushlstring(L, it->second.data(), it->second.size());
    }
    return 1;
}

int l_theme(lua_State* L)
{
    const auto& theme = self(L).host->services.theme;
    const std::string style = theme ? theme() : "dark";
    lua_pushlstring(L, style.data(), style.size());
    return 1;
}

int l_on_config(lua_State* L)
{
    Plugin& plugin = self(L);
    if (!lua_isnil(L, 1)) {
        luaL_checktype(L, 1, LUA_TFUNCTION);
    }
    luaL_unref(L, LUA_REGISTRYINDEX, plugin.config_ref);
    plugin.config_ref = LUA_NOREF;
    if (!lua_isnil(L, 1)) {
        lua_pushvalue(L, 1);
        plugin.config_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    return 0;
}

// ---- files and programs ----

int l_read_file(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    const lua_Integer asked =
            luaL_optinteger(L, 2, static_cast<lua_Integer>(default_file_limit));
    const size_t limit = asked < 0 ? 0 : static_cast<size_t>(asked);
    // not blocking: a FIFO without a writer must not stop the shell
    const int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    FILE* in = fd >= 0 ? fdopen(fd, "r") : nullptr;
    if (in == nullptr) {
        const int error = errno;
        if (fd >= 0) {
            close(fd);
        }
        lua_pushnil(L);
        lua_pushfstring(L, "%s: %s", path, strerror(error));
        return 2;
    }
    std::string text;
    char buffer[4096];
    int read_error = 0;           // a directory, EIO...
    while (text.size() < limit) { // /sys and /proc files have no size
        const size_t want = std::min(sizeof(buffer), limit - text.size());
        const size_t got = fread(buffer, 1, want, in);
        if (got < want && ferror(in) != 0) {
            read_error = errno;
        }
        text.append(buffer, got);
        if (got < want) {
            break;
        }
    }
    fclose(in);
    if (read_error != 0) {
        lua_pushnil(L);
        lua_pushfstring(L, "%s: %s", path, strerror(read_error));
        return 2;
    }
    lua_pushlstring(L, text.data(), text.size());
    return 1;
}

//! The names in a directory, sorted; nothing and the reason when it can't be
//! read
int l_list_dir(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    std::vector<std::string> names;
    int error = 0;
    if (!list_directory(path, names, &error)) {
        lua_pushnil(L);
        lua_pushfstring(L, "%s: %s", path, strerror(error));
        return 2;
    }
    lua_createtable(L, static_cast<int>(names.size()), 0);
    lua_Integer index = 1;
    for (const std::string& name : names) {
        lua_pushlstring(L, name.data(), name.size());
        lua_rawseti(L, -2, index++);
    }
    return 1;
}

int l_which(lua_State* L)
{
    const std::string name = luaL_checkstring(L, 1);
    if (name.empty()) {
        lua_pushnil(L);
        return 1;
    }
    if (name.find('/') != std::string::npos) {
        if (access(name.c_str(), X_OK) == 0) {
            lua_pushlstring(L, name.data(), name.size());
        } else {
            lua_pushnil(L);
        }
        return 1;
    }
    const char* path = getenv("PATH");
    for (const std::string& dir : split(path == nullptr ? "" : path, ':')) {
        std::string candidate = dir;
        candidate += '/';
        candidate += name;
        if (access(candidate.c_str(), X_OK) == 0) {
            lua_pushlstring(L, candidate.data(), candidate.size());
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

int launch_impl(lua_State* L)
{
    std::vector<std::string> argv;
    if (!string_list(L, 1, argv)) {
        return -1;
    }
    if (spawn(argv) < 0) {
        const int error = errno;
        lua_pushboolean(L, 0);
        lua_pushstring(L, strerror(error));
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

int l_launch(lua_State* L)
{
    return finish(L, launch_impl(L));
}

//! The function `ref` is called with `result` later, from the loop
void push_result(lua_State* L, const RunResult& result,
                 const std::string& error)
{
    lua_createtable(L, 0, 6);
    lua_pushboolean(L, result.ok ? 1 : 0);
    lua_setfield(L, -2, "ok");
    lua_pushinteger(L, result.status);
    lua_setfield(L, -2, "status");
    lua_pushlstring(L, result.output.data(), result.output.size());
    lua_setfield(L, -2, "stdout");
    lua_pushboolean(L, result.timed_out ? 1 : 0);
    lua_setfield(L, -2, "timed_out");
    lua_pushboolean(L, result.truncated ? 1 : 0);
    lua_setfield(L, -2, "truncated");
    if (!error.empty()) {
        lua_pushlstring(L, error.data(), error.size());
        lua_setfield(L, -2, "error");
    }
}

int exec_impl(lua_State* L)
{
    Plugin& plugin = self(L);
    if (!lua_isfunction(L, 2)) {
        return fail(L, "the second argument is a function");
    }
    std::vector<std::string> argv;
    if (!string_list(L, 1, argv)) {
        return -1;
    }
    Runner::Options options;
    if (lua_istable(L, 3)) {
        lua_getfield(L, 3, "timeout");
        if (lua_isnumber(L, -1)) {
            options.timeout = delay_of(lua_tonumber(L, -1), 1);
        }
        lua_pop(L, 1);
    }
    lua_pushvalue(L, 2);
    const int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    Plugin* const p = &plugin;
    const bool started = plugin.runner->run(
            argv,
            [p, ref](const RunResult& result) {
                push_result(p->L, result, "");
                call_ref(*p, ref, 1);
                luaL_unref(p->L, LUA_REGISTRYINDEX, ref);
            },
            options);
    if (!started) {
        // told later, as when the program had run: the function is never
        // called while the plugin is still in the call that asked for it
        const std::string error = strerror(errno);
        auto id = std::make_shared<Timers::Id>(0);
        *id = plugin.host->services.timers->after(0ms, [p, ref, error, id] {
            p->timers.erase(*id);
            push_result(p->L, RunResult{}, error);
            call_ref(*p, ref, 1);
            luaL_unref(p->L, LUA_REGISTRYINDEX, ref);
        });
        plugin.timers[*id] = LUA_NOREF; // the function is freed by the timer
    }
    return 0;
}

int l_exec(lua_State* L)
{
    return finish(L, exec_impl(L));
}

// ---- timers ----

//! The timer of a function; `every` is repeated
int start_timer(lua_State* L, bool every)
{
    Plugin& plugin = self(L);
    const lua_Number asked = luaL_checknumber(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    const auto delay = delay_of(asked, shortest_timer_ms);
    lua_pushvalue(L, 2);
    const int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    Plugin* const p = &plugin;
    auto id = std::make_shared<Timers::Id>(0);
    const auto fire = [p, ref, id, every] {
        if (!every) {
            p->timers.erase(*id);
        }
        call_ref(*p, ref);
        if (!every) {
            luaL_unref(p->L, LUA_REGISTRYINDEX, ref);
        }
    };
    Timers& timers = *plugin.host->services.timers;
    *id = every ? timers.every(delay, fire) : timers.after(delay, fire);
    plugin.timers[*id] = ref;

    auto* handle =
            static_cast<Timers::Id*>(lua_newuserdata(L, sizeof(Timers::Id)));
    *handle = *id;
    luaL_setmetatable(L, timer_meta);
    return 1;
}

int l_after(lua_State* L)
{
    return start_timer(L, false);
}

int l_every(lua_State* L)
{
    return start_timer(L, true);
}

int l_timer_cancel(lua_State* L)
{
    Plugin& plugin = self(L);
    const Timers::Id id =
            *static_cast<Timers::Id*>(luaL_checkudata(L, 1, timer_meta));
    const auto it = plugin.timers.find(id);
    if (it != plugin.timers.end()) {
        plugin.host->services.timers->cancel(id);
        luaL_unref(L, LUA_REGISTRYINDEX, it->second);
        plugin.timers.erase(it);
    }
    return 0;
}

// ---- widgets of the panel ----

Entry* check_entry(lua_State* L, int index)
{
    return *static_cast<Entry**>(luaL_checkudata(L, index, widget_meta));
}

int make_widget_impl(lua_State* L, bool button)
{
    Plugin& plugin = self(L);
    wm::Panel* const panel = plugin.host->services.panel;
    if (panel == nullptr || !panel->get_realized()) {
        return fail(L, "the panel is not there");
    }
    if (!lua_istable(L, 1)) {
        return fail(L, "a table is expected: {text = ..., sample = ..., icon = ...}");
    }
    std::string text;
    std::string sample;
    ItemIcon icon;
    if (!field_string(L, 1, "text", text) ||
        !field_string(L, 1, "sample", sample) ||
        !field_string(L, 1, "icon", icon.name) ||
        !field_string(L, 1, "icon_under", icon.under)) {
        return -1;
    }
    icon.icons = plugin.host->services.icons;
    icon.room = !icon.name.empty() || !icon.under.empty();

    auto entry = std::make_unique<Entry>();
    Entry* const raw = entry.get();
    if (button) {
        auto widget = std::make_unique<PluginButton>(text, sample, icon);
        raw->button = widget.get();
        raw->widget = std::move(widget);
        Plugin* const p = &plugin;
        raw->button->on_click.connect([p, raw](const XEvent&) {
            if (raw->click_ref != LUA_NOREF) {
                call_ref(*p, raw->click_ref);
            }
        });
    } else {
        auto widget = std::make_unique<PluginLabel>(text, sample, icon);
        raw->label = widget.get();
        raw->widget = std::move(widget);
    }
    panel->add_applet(raw->widget.get());
    raw->widget->set_events();
    plugin.entries.push_back(std::move(entry));

    if (button) {
        lua_getfield(L, 1, "on_click");
        if (lua_isfunction(L, -1)) {
            raw->click_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        } else {
            lua_pop(L, 1);
        }
    }
    auto** handle = static_cast<Entry**>(lua_newuserdata(L, sizeof(Entry*)));
    *handle = raw;
    luaL_setmetatable(L, widget_meta);
    return 1;
}

int l_label(lua_State* L)
{
    return finish(L, make_widget_impl(L, false));
}

int l_button(lua_State* L)
{
    return finish(L, make_widget_impl(L, true));
}

int l_widget_set_text(lua_State* L)
{
    Entry* entry = check_entry(L, 1);
    const std::string text = luaL_checkstring(L, 2);
    if (entry->label != nullptr) {
        entry->label->set_text(text);
    } else if (entry->button != nullptr) {
        entry->button->set_text(text);
    }
    return 0;
}

int l_widget_get_text(lua_State* L)
{
    const Entry* entry = check_entry(L, 1);
    std::string text;
    if (entry->label != nullptr) {
        text = entry->label->get_text();
    } else if (entry->button != nullptr) {
        text = entry->button->get_text();
    }
    lua_pushlstring(L, text.data(), text.size());
    return 1;
}

int l_widget_set_visible(lua_State* L)
{
    Entry* entry = check_entry(L, 1);
    luaL_checkany(L, 2);
    if (entry->widget != nullptr) {
        if (lua_toboolean(L, 2) != 0) {
            entry->widget->map_all();
        } else {
            entry->widget->unmap();
        }
    }
    return 0;
}

int l_widget_on_click(lua_State* L)
{
    Entry* entry = check_entry(L, 1);
    if (entry->button == nullptr) {
        return luaL_error(L, "a label can't be clicked, use tiny.panel.button");
    }
    if (!lua_isnil(L, 2)) {
        luaL_checktype(L, 2, LUA_TFUNCTION);
    }
    luaL_unref(L, LUA_REGISTRYINDEX, entry->click_ref);
    entry->click_ref = LUA_NOREF;
    if (!lua_isnil(L, 2)) {
        lua_pushvalue(L, 2);
        entry->click_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    return 0;
}

int l_widget_set_icon_impl(lua_State* L)
{
    Entry* entry = check_entry(L, 1);
    std::string name;
    std::string under;
    for (int i = 2; i <= 3; ++i) {
        if (!lua_isnoneornil(L, i) && lua_type(L, i) != LUA_TSTRING) {
            return fail(L, "the name of an icon is a text or nil");
        }
    }
    if (lua_isstring(L, 2) != 0) {
        name = lua_tostring(L, 2);
    }
    if (lua_isstring(L, 3) != 0) {
        under = lua_tostring(L, 3);
    }
    if (entry->label != nullptr) {
        entry->label->set_icon(name, under);
    } else if (entry->button != nullptr) {
        entry->button->set_icon(name, under);
    }
    return 0;
}

int l_widget_set_icon(lua_State* L)
{
    return finish(L, l_widget_set_icon_impl(L));
}

constexpr int popup_max_sections = 8;
constexpr int popup_max_rows = 40;

// The readers below leave the message on the top of the stack when they
// return false (the rest of the stack is dropped by the error)

bool read_row(lua_State* L, int index, PopupRow& row)
{
    if (!lua_istable(L, index)) {
        fail(L, "a row must be a table {icon, label, value}");
        return false;
    }
    return field_string(L, index, "icon", row.icon) &&
           field_string(L, index, "label", row.label) &&
           field_string(L, index, "value", row.value);
}

bool read_section(lua_State* L, int index, PopupSection& section)
{
    if (!lua_istable(L, index)) {
        fail(L, "a section must be a table {heading, rows}");
        return false;
    }
    if (!field_string(L, index, "heading", section.heading)) {
        return false;
    }
    lua_getfield(L, index, "rows");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return true;
    }
    if (!lua_istable(L, -1)) {
        fail(L, "rows must be a table");
        return false;
    }
    const lua_Integer count = static_cast<lua_Integer>(lua_rawlen(L, -1));
    if (count > popup_max_rows) {
        fail(L, "too many rows in a section");
        return false;
    }
    for (lua_Integer i = 1; i <= count; ++i) {
        lua_rawgeti(L, -1, i);
        PopupRow row;
        if (!read_row(L, lua_gettop(L), row)) {
            return false;
        }
        section.rows.push_back(std::move(row));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return true;
}

bool read_popup(lua_State* L, int index, PopupContent& content)
{
    if (!lua_istable(L, index)) {
        fail(L, "a table is expected: {title = ..., sections = ...}");
        return false;
    }
    if (!field_string(L, index, "icon", content.icon) ||
        !field_string(L, index, "title", content.title) ||
        !field_string(L, index, "subtitle", content.subtitle) ||
        !field_string(L, index, "value", content.value)) {
        return false;
    }
    lua_getfield(L, index, "sections");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return true;
    }
    if (!lua_istable(L, -1)) {
        fail(L, "sections must be a table");
        return false;
    }
    const lua_Integer count = static_cast<lua_Integer>(lua_rawlen(L, -1));
    if (count > popup_max_sections) {
        fail(L, "too many sections");
        return false;
    }
    for (lua_Integer i = 1; i <= count; ++i) {
        lua_rawgeti(L, -1, i);
        PopupSection section;
        if (!read_section(L, lua_gettop(L), section)) {
            return false;
        }
        content.sections.push_back(std::move(section));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return true;
}

int l_widget_popup_impl(lua_State* L)
{
    Plugin& plugin = self(L);
    Entry* entry = check_entry(L, 1);
    wm::Panel* const panel = plugin.host->services.panel;
    if (entry->widget == nullptr || panel == nullptr) {
        return fail(L, "the widget was destroyed");
    }
    PopupContent content;
    if (!read_popup(L, 2, content)) {
        return -1;
    }
    if (entry->popup == nullptr) {
        entry->popup =
                std::make_unique<PluginPopup>(plugin.host->services.icons);
        entry->popup->realize(panel->get_parent_window());
        entry->popup->set_events();
    }
    entry->popup->open(content, entry->widget->get_window(),
                       panel->get_window());
    return 0;
}

int l_widget_popup(lua_State* L)
{
    return finish(L, l_widget_popup_impl(L));
}

int l_widget_close_popup(lua_State* L)
{
    Entry* entry = check_entry(L, 1);
    if (entry->popup != nullptr) {
        entry->popup->unmap();
    }
    return 0;
}

//! Take the widget out of the panel. It may be destroyed from its own click,
//! so it is deleted a moment later.
void remove_widget(Plugin& plugin, Entry& entry)
{
    if (entry.widget == nullptr) {
        return;
    }
    plugin.host->services.panel->remove(entry.widget.get());
    entry.widget->unmap();
    luaL_unref(plugin.L, LUA_REGISTRYINDEX, entry.click_ref);
    entry.click_ref = LUA_NOREF;
    entry.label = nullptr;
    entry.button = nullptr;
    entry.popup.reset();
    plugin.graveyard.push_back(std::move(entry.widget));
    Plugin* const p = &plugin;
    auto id = std::make_shared<Timers::Id>(0);
    *id = plugin.host->services.timers->after(0ms, [p, id] {
        p->timers.erase(*id);
        p->graveyard.clear();
    });
    plugin.timers[*id] = LUA_NOREF;
}

int l_widget_destroy(lua_State* L)
{
    remove_widget(self(L), *check_entry(L, 1));
    return 0;
}

// ---- tiny.json ----

// the depth of the value is limited by the parser (json_max_depth)
// NOLINTNEXTLINE(misc-no-recursion)
void push_json(lua_State* L, const Json& value)
{
    luaL_checkstack(L, 4, "the JSON is too deep");
    switch (value.type) {
        case Json::Type::Null:
            lua_pushlightuserdata(L, nullptr);
            break;
        case Json::Type::Boolean:
            lua_pushboolean(L, value.boolean ? 1 : 0);
            break;
        case Json::Type::Number:
            lua_pushnumber(L, value.number);
            break;
        case Json::Type::String:
            lua_pushlstring(L, value.string.data(), value.string.size());
            break;
        case Json::Type::Array: {
            lua_createtable(L, static_cast<int>(value.array.size()), 0);
            lua_Integer index = 1;
            for (const Json& item : value.array) {
                push_json(L, item);
                lua_rawseti(L, -2, index++);
            }
            break;
        }
        case Json::Type::Object:
            lua_createtable(L, 0, static_cast<int>(value.object.size()));
            for (const auto& [key, item] : value.object) {
                push_json(L, item);
                lua_setfield(L, -2, key.c_str());
            }
            break;
    }
}

int l_json_decode(lua_State* L)
{
    size_t length = 0;
    const char* text = luaL_checklstring(L, 1, &length);
    std::string error;
    const auto value = parse_json(std::string(text, length), &error);
    if (!value) {
        lua_pushnil(L);
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    push_json(L, *value);
    return 1;
}

// ---- require ----

//! `require("name")` loads `name.lua` of the directory of the plugin
int require_impl(lua_State* L)
{
    Plugin& plugin = self(L);
    if (lua_type(L, 1) != LUA_TSTRING) {
        return fail(L, "the name of a module is a string");
    }
    const std::string name = lua_tostring(L, 1);
    const auto word = [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_';
    };
    const bool good = !name.empty() && name.front() != '.' &&
                      name.back() != '.' &&
                      name.find("..") == std::string::npos &&
                      std::all_of(name.begin(), name.end(),
                                  [&](char c) { return word(c) || c == '.'; });
    if (!good) {
        lua_pushfstring(L,
                        "the name of a module has letters, digits, _ and "
                        "dots: %s",
                        name.c_str());
        return -1;
    }
    lua_getfield(L, LUA_REGISTRYINDEX, "tiny.loaded");
    lua_getfield(L, -1, name.c_str());
    if (!lua_isnil(L, -1)) {
        return 1;
    }
    lua_pop(L, 1);
    if (!plugin.loading.insert(name).second) {
        lua_pushfstring(L, "the module %s requires itself", name.c_str());
        return -1;
    }
    std::string file = name;
    std::replace(file.begin(), file.end(), '.', '/');
    const std::string path = plugin.info.dir + "/" + file + ".lua";
    if (luaL_loadfile(L, path.c_str()) != LUA_OK) {
        plugin.loading.erase(name);
        return -1; // the message of Lua is on the stack
    }
    lua_pushstring(L, name.c_str());
    const int status = lua_pcall(L, 1, 1, 0);
    plugin.loading.erase(name); // also after an error, which goes on
    if (status != LUA_OK) {
        return -1;
    }
    if (lua_isnil(L, -1)) { // a module that returns nothing is `true`
        lua_pop(L, 1);
        lua_pushboolean(L, 1);
    }
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, name.c_str());
    return 1;
}

int l_require(lua_State* L)
{
    return finish(L, require_impl(L));
}

// ---- the state of a plugin ----

void register_class(lua_State* L, const char* name, const luaL_Reg* methods)
{
    luaL_newmetatable(L, name);
    lua_newtable(L);
    luaL_setfuncs(L, methods, 0);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);
}

void remove_function(lua_State* L, const char* table, const char* name)
{
    if (table == nullptr) {
        lua_pushnil(L);
        lua_setglobal(L, name);
        return;
    }
    lua_getglobal(L, table);
    lua_pushnil(L);
    lua_setfield(L, -2, name);
    lua_pop(L, 1);
}

void open_libraries(lua_State* L)
{
    // "_G" and not LUA_GNAME: Lua 5.3 does not have the macro
    const luaL_Reg libraries[] = {{"_G", luaopen_base},
                                  {LUA_TABLIBNAME, luaopen_table},
                                  {LUA_STRLIBNAME, luaopen_string},
                                  {LUA_MATHLIBNAME, luaopen_math},
                                  {LUA_UTF8LIBNAME, luaopen_utf8},
                                  {LUA_COLIBNAME, luaopen_coroutine},
                                  {LUA_OSLIBNAME, luaopen_os},
                                  {nullptr, nullptr}};
    for (const luaL_Reg* lib = libraries; lib->func != nullptr; ++lib) {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1);
    }
    // the plugin does what it needs through `tiny`
    remove_function(L, nullptr, "dofile");
    remove_function(L, nullptr, "loadfile");
    for (const char* name :
         {"execute", "remove", "rename", "tmpname", "exit", "setlocale"}) {
        remove_function(L, "os", name);
    }
}

const luaL_Reg tiny_functions[] = {{"log", l_log},
                                   {"setting", l_setting},
                                   {"config", l_config},
                                   {"theme", l_theme},
                                   {"on_config", l_on_config},
                                   {"after", l_after},
                                   {"every", l_every},
                                   {"exec", l_exec},
                                   {"launch", l_launch},
                                   {"read_file", l_read_file},
                                   {"list_dir", l_list_dir},
                                   {"which", l_which},
                                   {nullptr, nullptr}};

const luaL_Reg timer_methods[] = {{"cancel", l_timer_cancel},
                                  {nullptr, nullptr}};

const luaL_Reg widget_methods[] = {
        {"set_text", l_widget_set_text},       {"text", l_widget_get_text},
        {"set_visible", l_widget_set_visible}, {"on_click", l_widget_on_click},
        {"set_icon", l_widget_set_icon},       {"popup", l_widget_popup},
        {"close_popup", l_widget_close_popup}, {"destroy", l_widget_destroy},
        {nullptr, nullptr}};

void set_text_field(lua_State* L, const char* name, const std::string& value)
{
    lua_pushlstring(L, value.data(), value.size());
    lua_setfield(L, -2, name);
}

//! The global `tiny`
void make_module(Plugin& plugin)
{
    lua_State* L = plugin.L;
    lua_pushlightuserdata(L, &plugin);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &plugin_key);
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, "tiny.loaded");
    register_class(L, widget_meta, widget_methods);
    register_class(L, timer_meta, timer_methods);

    luaL_newlib(L, tiny_functions);
    lua_pushinteger(L, api_level);
    lua_setfield(L, -2, "api");
    set_text_field(L, "version", plugin.host->services.version);
    set_text_field(L, "platform", platform_name);

    lua_createtable(L, 0, 4);
    set_text_field(L, "id", plugin.info.id);
    set_text_field(L, "name", plugin.info.name);
    set_text_field(L, "version", plugin.info.version);
    set_text_field(L, "dir", plugin.info.dir);
    lua_setfield(L, -2, "plugin");

    lua_createtable(L, 0, 2);
    lua_pushcfunction(L, l_label);
    lua_setfield(L, -2, "label");
    lua_pushcfunction(L, l_button);
    lua_setfield(L, -2, "button");
    lua_setfield(L, -2, "panel");

    lua_createtable(L, 0, 2);
    lua_pushcfunction(L, l_json_decode);
    lua_setfield(L, -2, "decode");
    lua_pushlightuserdata(L, nullptr);
    lua_setfield(L, -2, "null");
    lua_setfield(L, -2, "json");

    lua_setglobal(L, "tiny");
    lua_pushcfunction(L, l_log);
    lua_setglobal(L, "print");
    lua_pushcfunction(L, l_require);
    lua_setglobal(L, "require");
}

//! The plugin that runs with the id, or null
Plugin* find_running(const HostState& host, const std::string& id)
{
    for (const auto& plugin : host.running) {
        if (plugin->info.id == id) {
            return plugin.get();
        }
    }
    return nullptr;
}

//! Free what a plugin holds; the plugin is not used after it
void shutdown(Plugin& plugin)
{
    // the programs first: nothing may call into a state that is closing
    plugin.runner.reset();
    Timers& timers = *plugin.host->services.timers;
    for (const auto& [id, ref] : plugin.timers) {
        timers.cancel(id);
    }
    plugin.timers.clear();
    for (const auto& entry : plugin.entries) {
        if (entry->widget != nullptr) {
            plugin.host->services.panel->remove(entry->widget.get());
        }
    }
    plugin.entries.clear();
    plugin.graveyard.clear();
    if (plugin.L != nullptr) {
        lua_close(plugin.L);
        plugin.L = nullptr;
    }
}

//! Start a plugin: its own state, the module `tiny` and `plugin.lua`
std::unique_ptr<Plugin> start(HostState& host, const PluginInfo& info,
                              std::string& error)
{
    auto plugin = std::make_unique<Plugin>();
    plugin->host = &host;
    plugin->info = info;
    const auto config = host.config.plugins.find(info.id);
    if (config != host.config.plugins.end()) {
        plugin->values = config->second.values;
    }
    plugin->runner = std::make_unique<Runner>(*host.services.timers,
                                              *host.services.watches);
    plugin->L = luaL_newstate();
    if (plugin->L == nullptr) {
        error = "out of memory";
        return nullptr;
    }
    open_libraries(plugin->L);
    make_module(*plugin);
    lua_sethook(plugin->L, time_hook, LUA_MASKCOUNT, hook_every);

    const std::string main = info.dir + "/plugin.lua";
    if (luaL_loadfile(plugin->L, main.c_str()) != LUA_OK) {
        error = lua_tostring(plugin->L, -1);
        shutdown(*plugin);
        return nullptr;
    }
    // the same call as of the other functions, but the error is kept
    lua_State* L = plugin->L;
    lua_pushcfunction(L, traceback);
    lua_insert(L, -2);
    plugin->depth = 1;
    plugin->call_start = std::chrono::steady_clock::now();
    const int status = lua_pcall(L, 0, 0, -2);
    plugin->depth = 0;
    if (status != LUA_OK) {
        error = lua_tostring(L, -1);
        shutdown(*plugin);
        return nullptr;
    }
    lua_pop(L, 1); // the function of the traceback
    return plugin;
}

} // namespace

struct PluginHost::Impl {
    HostState state;
};

PluginHost::PluginHost(HostServices services): impl(std::make_unique<Impl>())
{
    impl->state.services = std::move(services);
}

PluginHost::~PluginHost()
{
    stop();
}

void PluginHost::stop()
{
    HostState& host = impl->state;
    // the last started goes first
    while (!host.running.empty()) {
        shutdown(*host.running.back());
        host.running.pop_back();
    }
}

std::vector<std::string> PluginHost::running() const
{
    std::vector<std::string> ids;
    for (const auto& plugin : impl->state.running) {
        ids.push_back(plugin->info.id);
    }
    return ids;
}

std::string PluginHost::error_of(const std::string& id) const
{
    const auto it = impl->state.errors.find(id);
    return it == impl->state.errors.end() ? "" : it->second;
}

bool PluginHost::apply(const std::vector<PluginInfo>& found,
                       const wm::Config& config, bool force_restart)
{
    HostState& host = impl->state;
    std::vector<HostState::Attempt> wanted;
    std::vector<const PluginInfo*> infos;
    for (const PluginInfo& info : found) {
        if (!plugin_enabled(info, config)) {
            continue;
        }
        HostState::Attempt attempt{info.id, info.dir, info.version, {}};
        const auto it = config.plugins.find(info.id);
        if (it != config.plugins.end()) {
            attempt.values = it->second.values;
        }
        wanted.push_back(std::move(attempt));
        infos.push_back(&info);
    }

    const auto same_plugin = [](const HostState::Attempt& a,
                                const HostState::Attempt& b) {
        return a.id == b.id && a.dir == b.dir && a.version == b.version;
    };
    bool restart = force_restart || wanted.size() != host.attempts.size() ||
                   !std::equal(wanted.begin(), wanted.end(),
                               host.attempts.begin(), same_plugin);
    const bool changed = host.config.raw != config.raw;
    std::vector<std::string> to_tell; //!< ids of the plugins to tell
    if (!restart) {
        for (size_t i = 0; i < wanted.size(); ++i) {
            const Plugin* const running = find_running(host, wanted[i].id);
            if (wanted[i].values != host.attempts[i].values) {
                // one that can't follow the change starts again; so does one
                // that failed, the user may have mended what was wrong
                if (running == nullptr || running->config_ref == LUA_NOREF) {
                    restart = true;
                } else {
                    to_tell.push_back(wanted[i].id);
                }
            } else if (changed && running != nullptr &&
                       running->config_ref != LUA_NOREF) {
                to_tell.push_back(wanted[i].id);
            }
        }
    }

    host.config = config;
    if (restart) {
        stop();
        host.errors.clear();
        host.attempts = wanted;
        if (host.services.panel == nullptr ||
            !host.services.panel->get_realized()) {
            if (!wanted.empty() && !host.told_no_panel) {
                host.told_no_panel = true;
                log_line(host, "tiny-shell: no panel, the plugins do not run");
            }
            return true;
        }
        for (const PluginInfo* info : infos) {
            std::string error;
            auto plugin = start(host, *info, error);
            if (plugin) {
                host.running.push_back(std::move(plugin));
            } else {
                host.errors[info->id] = error;
                log_line(host, "tiny-shell: plugin " + info->id +
                                       " did not start: " + error);
            }
        }
        return true;
    }
    host.attempts = wanted;
    for (const std::string& id : to_tell) {
        Plugin* const plugin = find_running(host, id);
        const auto it = config.plugins.find(id);
        plugin->values = it == config.plugins.end()
                                 ? std::map<std::string, std::string>{}
                                 : it->second.values;
        call_ref(*plugin, plugin->config_ref);
    }
    return false;
}

} // namespace tiny::plugin

#endif // TINY_HAVE_LUA
