#include "wm_helpers.h"

#include <poll.h>

#include <chrono>

#include "plugin/host.h"
#include "plugin/plugin_info.h"

using namespace std::chrono_literals;
using namespace tiny::plugin;

namespace {

//! The loop of the shell without the display: timers and watched descriptors
class Loop {
  public:
    tiny::Timers timers;
    tiny::FdWatch watches;

    template <class Done>
    bool run_until(Done done, std::chrono::milliseconds limit = 5s)
    {
        const auto end = tiny::Timers::Clock::now() + limit;
        while (!done() && tiny::Timers::Clock::now() < end) {
            std::vector<pollfd> fds;
            std::vector<tiny::FdWatch::Id> ids;
            watches.prepare(fds, ids);
            const int wait = std::min(timers.timeout_ms(), 20);
            poll(fds.data(), fds.size(), wait < 0 ? 20 : wait);
            timers.run_due();
            watches.dispatch(fds, ids, 0);
        }
        return done();
    }
};

//! A directory with the plugins, removed at the end
class PluginTree {
  public:
    PluginTree()
    {
        static int counter = 0;
        path = std::filesystem::temp_directory_path() /
               ("tiny-host-" + std::to_string(getpid()) + "-" +
                std::to_string(counter++));
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
    }
    ~PluginTree() { std::filesystem::remove_all(path); }
    PluginTree(const PluginTree&) = delete;
    PluginTree& operator=(const PluginTree&) = delete;

    void add(const std::string& id, const std::string& lua,
             const std::string& ini = "[plugin]\nenabled = yes\n",
             const std::string& file = "plugin.lua")
    {
        std::filesystem::create_directories(path / id);
        std::ofstream(path / id / "plugin.ini") << ini;
        std::ofstream(path / id / file) << lua;
    }

    std::vector<PluginInfo> find() const
    {
        PluginDirs dirs;
        dirs.user = path.string();
        return find_plugins(dirs);
    }

    std::filesystem::path path;
};

class HostTest : public ::testing::Test {
  protected:
    HostTest()
    {
        panel.realize(root_window(), 0, 0);
        services.panel = &panel;
        services.timers = &loop.timers;
        services.watches = &loop.watches;
        services.version = "9.8";
        services.theme = [this] { return theme; };
        services.log = [this](const std::string& line) {
            lines.push_back(line);
        };
    }

    //! Whether a line of the log has the text
    bool logged(const std::string& text) const
    {
        return std::any_of(lines.begin(), lines.end(),
                           [&](const std::string& line) {
                               return line.find(text) != std::string::npos;
                           });
    }

    size_t panel_children() { return children_of(panel.get_window()).size(); }

    const WidgetFont font;
    const ShellFonts shell_fonts;
    tiny::wm::Panel panel;
    Loop loop;
    HostServices services;
    std::string theme = "dark";
    std::vector<std::string> lines;
    PluginTree tree;
    tiny::wm::Config config;
};

} // namespace

#ifdef TINY_HAVE_LUA

TEST_F(HostTest, is_available_with_lua)
{
    EXPECT_TRUE(PluginHost::available());
}

TEST_F(HostTest, runs_an_enabled_plugin_and_gives_it_the_module_tiny)
{
    tree.add("hello", "tiny.log(tiny.api, tiny.version, tiny.plugin.id, "
                      "tiny.plugin.name, tiny.plugin.version, tiny.theme())\n"
                      "print('printed')\n");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_EQ(host.running(), std::vector<std::string>{"hello"});
    EXPECT_TRUE(logged("plugin hello: 1 9.8 hello hello  dark"));
    EXPECT_TRUE(logged("plugin hello: printed"));
}

TEST_F(HostTest, a_plugin_that_is_not_enabled_does_not_run)
{
    tree.add("off", "tiny.log('ran')", "[plugin]\nenabled = no\n");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(host.running().empty());
    EXPECT_FALSE(logged("ran"));
    config.plugins["off"].enabled = true; // the user switches it on
    host.apply(tree.find(), config);
    EXPECT_EQ(host.running(), std::vector<std::string>{"off"});
    config.plugins["off"].enabled = false;
    host.apply(tree.find(), config);
    EXPECT_TRUE(host.running().empty());
}

TEST_F(HostTest, an_error_stops_only_the_plugin_that_made_it)
{
    tree.add("a_broken", "error('boom')");
    tree.add("b_fine", "tiny.log('fine')");
    tree.add("c_syntax", "this is not lua");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_EQ(host.running(), std::vector<std::string>{"b_fine"});
    EXPECT_NE(host.error_of("a_broken").find("boom"), std::string::npos);
    EXPECT_FALSE(host.error_of("c_syntax").empty());
    EXPECT_TRUE(host.error_of("b_fine").empty());
    EXPECT_TRUE(logged("plugin a_broken did not start"));
}

TEST_F(HostTest, a_plugin_that_runs_too_long_is_stopped)
{
    tree.add("loop", "while true do end");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(host.running().empty());
    EXPECT_NE(host.error_of("loop").find("too long"), std::string::npos);
}

TEST_F(HostTest, a_plugin_has_no_files_and_no_programs_of_its_own)
{
    tree.add("sandbox",
             "tiny.log(io, os.execute, os.remove, os.exit, dofile, loadfile, "
             "package, type(os.time), type(string.format))");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(logged("nil nil nil nil nil nil nil function function"));
}

TEST_F(HostTest, every_plugin_has_its_own_state)
{
    tree.add("one", "shared = 1");
    tree.add("two", "tiny.log(tostring(shared))");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(logged("plugin two: nil"));
}

TEST_F(HostTest, timers_call_the_functions_and_can_be_cancelled)
{
    tree.add("timers",
             "n = 0\n"
             "local t\n"
             "t = tiny.every(10, function()\n"
             "  n = n + 1\n"
             "  if n == 3 then t:cancel() tiny.log('every done') end\n"
             "end)\n"
             "tiny.after(10, function() tiny.log('after') end)\n"
             "local c = tiny.after(10, function() tiny.log('never') end)\n"
             "c:cancel()\n");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(loop.run_until([&] { return logged("every done"); }));
    EXPECT_TRUE(logged("plugin timers: after"));
    EXPECT_FALSE(logged("never"));
    loop.run_until([] { return false; }, 100ms);
    EXPECT_TRUE(loop.timers.empty());
}

TEST_F(HostTest, an_error_in_a_function_is_logged_and_the_plugin_goes_on)
{
    tree.add("bad", "tiny.every(10, function() error('tick') end)");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(loop.run_until([&] { return logged("tick"); }));
    EXPECT_EQ(host.running(), std::vector<std::string>{"bad"});
}

TEST_F(HostTest, exec_runs_a_program_and_tells_the_result_later)
{
    tree.add("run",
             "tiny.exec({'sh', '-c', 'printf out; exit 2'}, function(r)\n"
             "  tiny.log('ok=' .. tostring(r.ok), 'status=' .. r.status,\n"
             "           'out=' .. r.stdout, tostring(r.timed_out))\n"
             "end)\n"
             "tiny.log('after the call')\n");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(logged("after the call"));
    EXPECT_FALSE(logged("status="));
    EXPECT_TRUE(loop.run_until([&] { return logged("status="); }));
    EXPECT_TRUE(logged("ok=false status=2 out=out false"));
}

TEST_F(HostTest, exec_that_can_not_start_tells_it_later_too)
{
    tree.add("missing", "tiny.exec({'/nonexistent/tiny-program'}, function(r)\n"
                        "  tiny.log('result', tostring(r.ok), r.error)\n"
                        "end)\n"
                        "tiny.log('first')\n");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_FALSE(logged("result"));
    EXPECT_TRUE(loop.run_until([&] { return logged("result"); }));
    EXPECT_TRUE(logged("result false No such file or directory"));
}

TEST_F(HostTest, exec_stops_a_program_after_the_timeout)
{
    tree.add("slow", "tiny.exec({'sleep', '30'}, function(r)\n"
                     "  tiny.log('timed_out=' .. tostring(r.timed_out))\n"
                     "end, {timeout = 100})\n");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(loop.run_until([&] { return logged("timed_out=true"); }));
}

TEST_F(HostTest, exec_wants_a_list_of_strings_and_a_function)
{
    tree.add("bad1", "tiny.exec('ls', function() end)");
    tree.add("bad2", "tiny.exec({'ls', 1}, function() end)");
    tree.add("bad3", "tiny.exec({}, function() end)");
    tree.add("bad4", "tiny.exec({'ls'})");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(host.running().empty());
    EXPECT_FALSE(host.error_of("bad1").empty());
    EXPECT_NE(host.error_of("bad2").find("strings"), std::string::npos);
    EXPECT_NE(host.error_of("bad3").find("missing"), std::string::npos);
    EXPECT_FALSE(host.error_of("bad4").empty());
}

TEST_F(HostTest, programs_of_a_stopped_plugin_are_killed_and_never_report)
{
    tree.add("gone",
             "tiny.exec({'sleep', '30'}, function() tiny.log('late') end)");
    PluginHost host(services);
    host.apply(tree.find(), config);
    host.stop();
    EXPECT_TRUE(loop.watches.empty());
    loop.run_until([] { return false; }, 100ms);
    EXPECT_FALSE(logged("late"));
}

TEST_F(HostTest, launch_starts_a_program_and_tells_a_failure)
{
    tree.add("launch",
             "tiny.log(tiny.launch({'true'}))\n"
             "tiny.log(tiny.launch({'/nonexistent/tiny-program'}))\n");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(logged("plugin launch: true"));
    EXPECT_TRUE(logged("plugin launch: false No such file or directory"));
}

TEST_F(HostTest, reads_files_and_finds_programs)
{
    const std::string data = (tree.path / "data.txt").string();
    std::ofstream(data) << "0123456789";
    tree.add(
            "files",
            "tiny.log(tiny.read_file('" + data +
                    "'))\n"
                    "tiny.log(tiny.read_file('" +
                    data +
                    "', 4))\n"
                    "tiny.log(tiny.read_file('/nonexistent/tiny-file'))\n"
                    "tiny.log(tiny.which('sh') ~= nil, tiny.which('/bin/sh'),\n"
                    "         tiny.which('nonexistent-tiny-program'), "
                    "tiny.which(''))\n");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(logged("plugin files: 0123456789"));
    EXPECT_TRUE(logged("plugin files: 0123"));
    EXPECT_TRUE(logged("plugin files: nil /nonexistent/tiny-file: No such "
                       "file or directory"));
    EXPECT_TRUE(logged("plugin files: true /bin/sh nil nil"));
}

TEST_F(HostTest, lists_a_directory_and_knows_the_platform)
{
    std::filesystem::create_directories(tree.path / "d" / "sub");
    std::ofstream(tree.path / "d" / "b.txt") << "x";
    std::ofstream(tree.path / "d" / "a.txt") << "x";
    tree.add("dirs",
             "local names = tiny.list_dir('" + (tree.path / "d").string() +
                     "')\n"
                     "tiny.log(#names, names[1], names[2], names[3])\n"
                     "tiny.log(tiny.list_dir('/nonexistent/tiny-dir'))\n"
                     "tiny.log(tiny.platform)\n");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(logged("plugin dirs: 3 a.txt b.txt sub"));
    EXPECT_TRUE(logged("plugin dirs: nil /nonexistent/tiny-dir: "));
#if defined(__linux__)
    EXPECT_TRUE(logged("plugin dirs: linux"));
#endif
    EXPECT_FALSE(logged("plugin dirs: unknown"));
}

TEST_F(HostTest, decodes_json)
{
    tree.add("json",
             "local v = tiny.json.decode('{\"a\": [1, 2.5, \"x\", true, null],"
             " \"b\": {\"c\": \"d\"}}')\n"
             "tiny.log(#v.a, v.a[2], v.a[3], v.a[4], v.a[5] == tiny.json.null,"
             " v.b.c)\n"
             "tiny.log(tiny.json.decode('[1,'))\n");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(logged("plugin json: 5 2.5 x true true d"));
    EXPECT_TRUE(logged("plugin json: nil "));
    EXPECT_TRUE(logged("ends too soon"));
}

TEST_F(HostTest, gives_the_settings_the_type_of_their_schema)
{
    tree.add("typed",
             "tiny.log(type(tiny.setting('flag')), tiny.setting('flag'),\n"
             "         type(tiny.setting('count')), tiny.setting('count'),\n"
             "         tiny.setting('words'), tiny.setting('pick'),\n"
             "         tiny.setting('unknown'))\n",
             "[plugin]\nenabled = yes\nsettings = flag;count;words;pick\n"
             "[setting.flag]\ntype = bool\n"
             "[setting.count]\ntype = number\ndefault = 3\n"
             "[setting.words]\ntype = text\ndefault = hi there\n"
             "[setting.pick]\ntype = choice\nchoices = a;b\n");
    config.plugins["typed"].values["flag"] = "yes";
    config.plugins["typed"].values["count"] = "50x"; // wrong: the default
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(logged("plugin typed: boolean true number 3 hi there a nil"));
}

TEST_F(HostTest, reads_the_settings_of_the_shell)
{
    tree.add("shell", "tiny.log(tiny.config('theme', 'style'),\n"
                      "         tiny.config('theme', 'nothing'))");
    config.raw[{"theme", "style"}] = "light";
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(logged("plugin shell: light nil"));
}

TEST_F(HostTest, require_loads_modules_of_the_plugin_only)
{
    tree.add("mods", "local util = require('util')\n"
                     "tiny.log(util.twice(4), require('util') == util)\n"
                     "tiny.log(pcall(require, '../x'))\n"
                     "tiny.log(pcall(require, 'a/b'))\n"
                     "tiny.log(pcall(require, 'nothing'))\n"
                     "tiny.log(require('lib.deep'))\n"
                     "tiny.log(pcall(require, 'loop'))\n");
    std::ofstream(tree.path / "mods" / "util.lua")
            << "return {twice = function(x) return x * 2 end}";
    std::filesystem::create_directories(tree.path / "mods" / "lib");
    std::ofstream(tree.path / "mods" / "lib" / "deep.lua")
            << "return 'deep ' .. ...";
    std::ofstream(tree.path / "mods" / "loop.lua") << "require('loop')";
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(logged("plugin mods: 8 true"));
    EXPECT_TRUE(logged("plugin mods: false the name of a module"));
    EXPECT_TRUE(logged("plugin mods: deep lib.deep"));
    EXPECT_TRUE(logged("requires itself"));
    EXPECT_TRUE(logged("cannot open")) << "a module that is not there";
}

TEST_F(HostTest, a_label_is_in_the_panel_and_follows_its_plugin)
{
    tree.add("label",
             "label = tiny.panel.label{text = '12:00', sample = '00:00'}\n"
             "tiny.log(label:text())\n"
             "label:set_text('13:30')\n"
             "tiny.log(label:text())\n");
    const size_t before = panel_children();
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_EQ(panel_children(), before + 1);
    EXPECT_TRUE(logged("plugin label: 12:00"));
    EXPECT_TRUE(logged("plugin label: 13:30"));
    host.stop();
    EXPECT_EQ(panel_children(), before);
}

TEST_F(HostTest, a_widget_is_made_as_wide_as_its_longest_text)
{
    tree.add("wide", "tiny.panel.label{text = 'a', sample = 'wide sample'}\n"
                     "tiny.panel.label{text = 'a'}\n");
    const auto before = children_of(panel.get_window());
    PluginHost host(services);
    host.apply(tree.find(), config);
    const auto after = children_of(panel.get_window());
    ASSERT_EQ(after.size(), before.size() + 2);
    const int wide = attributes_of(after[after.size() - 2]).width;
    const int narrow = attributes_of(after.back()).width;
    EXPECT_GT(wide, narrow);
}

TEST_F(HostTest, a_button_tells_its_click_to_the_plugin)
{
    tree.add("button",
             "clicks = 0\n"
             "b = tiny.panel.button{text = 'go', on_click = function()\n"
             "  clicks = clicks + 1\n"
             "  tiny.log('clicked', clicks)\n"
             "end}\n"
             "b:on_click(function() tiny.log('replaced') end)\n");
    const auto before = children_of(panel.get_window());
    PluginHost host(services);
    host.apply(tree.find(), config);
    const auto after = children_of(panel.get_window());
    ASSERT_EQ(after.size(), before.size() + 1);
    send(after.back(), make_event(EnterNotify));
    send(after.back(), make_event(ButtonRelease));
    EXPECT_TRUE(logged("plugin button: replaced"));
    EXPECT_FALSE(logged("clicked"));
}

TEST_F(HostTest, a_label_cannot_be_clicked)
{
    tree.add("label", "tiny.panel.label{text = 'x'}:on_click(function() end)");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_NE(host.error_of("label").find("can't be clicked"),
              std::string::npos);
}

TEST_F(HostTest, a_widget_can_be_hidden_and_destroyed_also_from_its_own_click)
{
    tree.add("gone", "b = tiny.panel.button{text = 'x', on_click = function()\n"
                     "  b:destroy()\n"
                     "  b:set_text('late') -- no error, the widget is gone\n"
                     "  tiny.log('destroyed')\n"
                     "end}\n"
                     "l = tiny.panel.label{text = 'y'}\n"
                     "l:set_visible(false)\n"
                     "l:set_visible(true)\n");
    const auto before = children_of(panel.get_window());
    PluginHost host(services);
    host.apply(tree.find(), config);
    const auto after = children_of(panel.get_window());
    ASSERT_EQ(after.size(), before.size() + 2);
    send(after[after.size() - 2], make_event(EnterNotify));
    send(after[after.size() - 2], make_event(ButtonRelease));
    EXPECT_TRUE(logged("destroyed"));
    EXPECT_TRUE(loop.run_until(
            [&] { return panel_children() == before.size() + 1; }));
    EXPECT_EQ(host.running(), std::vector<std::string>{"gone"});
}

TEST_F(HostTest, the_widgets_need_a_panel)
{
    tree.add("nopanel", "tiny.panel.label{text = 'x'}");
    services.panel = nullptr;
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(host.running().empty());
    EXPECT_TRUE(logged("no panel"));
}

TEST_F(HostTest, the_same_configuration_does_not_start_the_plugins_again)
{
    tree.add("count", "tiny.log('started')");
    PluginHost host(services);
    EXPECT_TRUE(host.apply(tree.find(), config));
    EXPECT_FALSE(host.apply(tree.find(), config)); // nothing was restarted
    EXPECT_EQ(std::count_if(lines.begin(), lines.end(),
                            [](const std::string& line) {
                                return line.find("started") !=
                                       std::string::npos;
                            }),
              1);
}

TEST_F(HostTest, a_new_plugin_starts_all_of_them_again_in_the_order_of_ids)
{
    tree.add("b", "tiny.log('b')");
    PluginHost host(services);
    host.apply(tree.find(), config);
    tree.add("a", "tiny.log('a')");
    host.apply(tree.find(), config);
    EXPECT_EQ(host.running(), (std::vector<std::string>{"a", "b"}));
}

TEST_F(HostTest, a_plugin_without_on_config_starts_again_for_a_new_setting)
{
    tree.add("plain", "tiny.log('start ' .. tiny.setting('words'))",
             "[plugin]\nenabled = yes\nsettings = words\n"
             "[setting.words]\ntype = text\ndefault = x\n");
    PluginHost host(services);
    host.apply(tree.find(), config);
    config.plugins["plain"].values["words"] = "y";
    host.apply(tree.find(), config);
    EXPECT_TRUE(logged("plugin plain: start x"));
    EXPECT_TRUE(logged("plugin plain: start y"));
}

TEST_F(HostTest, a_plugin_with_on_config_is_told_and_goes_on)
{
    tree.add("live",
             "tiny.log('start')\n"
             "tiny.on_config(function()\n"
             "  tiny.log('config', tiny.setting('words'), tiny.theme(),\n"
             "           tiny.config('theme', 'style'))\n"
             "end)\n",
             "[plugin]\nenabled = yes\nsettings = words\n"
             "[setting.words]\ntype = text\ndefault = x\n");
    PluginHost host(services);
    host.apply(tree.find(), config);
    config.plugins["live"].values["words"] = "y";
    theme = "light";
    config.raw[{"theme", "style"}] = "light";
    EXPECT_FALSE(host.apply(tree.find(), config)); // told, not started again
    EXPECT_TRUE(logged("plugin live: config y light light"));
    EXPECT_EQ(std::count_if(lines.begin(), lines.end(),
                            [](const std::string& line) {
                                return line.find(": start") !=
                                       std::string::npos;
                            }),
              1);
    // another setting of the shell is told too, a repeated apply is not
    lines.clear();
    config.raw[{"dock", "mode"}] = "on";
    host.apply(tree.find(), config);
    EXPECT_TRUE(logged("plugin live: config"));
    lines.clear();
    host.apply(tree.find(), config);
    EXPECT_FALSE(logged("config"));
}

TEST_F(HostTest, a_plugin_that_failed_is_tried_again_when_its_setting_changes)
{
    tree.add("retry", "assert(tiny.setting('words') == 'ok', 'not ok')",
             "[plugin]\nenabled = yes\nsettings = words\n"
             "[setting.words]\ntype = text\ndefault = bad\n");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_FALSE(host.error_of("retry").empty());
    host.apply(tree.find(), config); // nothing changed: not tried again
    EXPECT_EQ(std::count_if(lines.begin(), lines.end(),
                            [](const std::string& line) {
                                return line.find("did not start") !=
                                       std::string::npos;
                            }),
              1);
    config.plugins["retry"].values["words"] = "ok";
    host.apply(tree.find(), config);
    EXPECT_EQ(host.running(), std::vector<std::string>{"retry"});
    EXPECT_TRUE(host.error_of("retry").empty());
}

TEST_F(HostTest, the_destructor_stops_the_plugins)
{
    tree.add("bye",
             "tiny.panel.label{text = 'x'}\ntiny.every(10, function() end)");
    const size_t before = panel_children();
    {
        PluginHost host(services);
        host.apply(tree.find(), config);
        EXPECT_EQ(panel_children(), before + 1);
    }
    EXPECT_EQ(panel_children(), before);
    EXPECT_TRUE(loop.timers.empty());
}

TEST_F(HostTest, on_config_can_be_dropped)
{
    tree.add("drop", "tiny.on_config(function() tiny.log('called') end)\n"
                     "tiny.on_config(nil)\n");
    PluginHost host(services);
    host.apply(tree.find(), config);
    config.raw[{"a", "b"}] = "c";
    host.apply(tree.find(), config);
    EXPECT_FALSE(logged("called"));
}

//! The plugins of data/ are what the shell installs: they must be good
TEST_F(HostTest, the_plugins_of_the_shell_are_good_and_start)
{
    PluginDirs dirs;
    dirs.system = {"data/tiny-shell/plugins"};
    const auto found = find_plugins(dirs);
    if (found.empty()) {
        GTEST_SKIP() << "run from the directory of the sources";
    }
    ASSERT_GE(found.size(), 2u);
    tiny::wm::Config all;
    for (const PluginInfo& info : found) {
        EXPECT_EQ(info.problem, "") << info.id;
        EXPECT_FALSE(info.name.empty());
        EXPECT_FALSE(info.description.empty()) << info.id;
        EXPECT_FALSE(info.version.empty()) << info.id;
        all.plugins[info.id].enabled = true; // also the ones that are off
    }
    PluginHost host(services);
    host.apply(found, all);
    for (const PluginInfo& info : found) {
        EXPECT_EQ(host.error_of(info.id), "") << info.id;
    }
    EXPECT_EQ(host.running().size(), found.size());
    host.stop();
}

#else // no Lua

TEST_F(HostTest, without_lua_nothing_runs_and_the_user_is_told_once)
{
    EXPECT_FALSE(PluginHost::available());
    tree.add("hello", "tiny.log('ran')");
    config.plugins["hello"].enabled = true; // the user switched it on
    PluginHost host(services);
    host.apply(tree.find(), config);
    host.apply(tree.find(), config);
    EXPECT_TRUE(host.running().empty());
    EXPECT_TRUE(host.error_of("hello").empty());
    EXPECT_EQ(lines.size(), 1u);
    EXPECT_NE(lines[0].find("no Lua"), std::string::npos);
    host.stop();
}

TEST_F(HostTest, without_lua_a_plugin_that_nobody_switched_on_is_not_mentioned)
{
    tree.add("quiet", "", "[plugin]\nenabled = yes\n");
    config.plugins["other"].enabled = false;
    config.plugins["third"].values["a"] = "b"; // only settings
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(lines.empty());
}

#endif
