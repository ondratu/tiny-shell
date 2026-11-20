#include "host_fixture.h"

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

TEST_F(HostTest, a_pcall_does_not_keep_a_plugin_running_for_ever)
{
    tree.add("swallow",
             "while true do pcall(function() while true do end end) end");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(host.running().empty());
    EXPECT_NE(host.error_of("swallow").find("too long"), std::string::npos);
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

TEST_F(HostTest, a_delay_that_is_not_a_number_or_too_long_is_made_safe)
{
    tree.add("delays",
             "tiny.after(0/0, function() tiny.log('nan fired') end)\n"
             "tiny.after(math.huge, function() tiny.log('huge fired') end)\n"
             "tiny.after(1e300, function() tiny.log('big fired') end)\n"
             "tiny.exec({'true'}, function(r)\n"
             "  tiny.log('exec', r.ok, r.timed_out)\n"
             "end, {timeout = 1e300})\n");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(loop.run_until(
            [&] { return logged("nan fired") && logged("exec true false"); }));
    EXPECT_FALSE(logged("huge fired"));
    EXPECT_FALSE(logged("big fired"));
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

TEST_F(HostTest, exec_gives_a_program_the_text_for_its_input)
{
    tree.add(
            "input",
            "tiny.exec({'cat'}, function(r) tiny.log('got=' .. r.stdout) end,\n"
            "          {stdin = 'pass word'})\n"
            "tiny.exec({'cat'}, function(r) tiny.log('none=' .. r.stdout) "
            "end)\n");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(loop.run_until(
            [&] { return logged("got=pass word") && logged("none="); }));
    EXPECT_FALSE(logged("none=pass"));
}

TEST_F(HostTest, exec_with_an_input_that_is_not_a_text_or_too_long_is_an_error)
{
    tree.add("bad", "local ok = pcall(tiny.exec, {'cat'}, function() end,\n"
                    "                 {stdin = {}})\n"
                    "tiny.log('table=' .. tostring(ok))\n"
                    "tiny.exec({'cat'}, function(r)\n"
                    "  tiny.log('long=' .. tostring(r.ok), r.error)\n"
                    "end, {stdin = string.rep('x', 5000)})\n");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(logged("table=false"));
    EXPECT_TRUE(loop.run_until([&] { return logged("long=false"); }));
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

TEST_F(HostTest, read_file_tells_a_directory_and_does_not_wait_for_a_fifo)
{
    const std::string fifo = (tree.path / "fifo").string();
    ASSERT_EQ(mkfifo(fifo.c_str(), 0600), 0);
    tree.add("special", "tiny.log('dir', tiny.read_file('" +
                                tree.path.string() +
                                "'))\n"
                                "tiny.log('fifo', tiny.read_file('" +
                                fifo + "'))\n");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(logged("plugin special: dir nil " + tree.path.string() +
                       ": Is a directory"));
    EXPECT_TRUE(logged("plugin special: fifo "));
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
#ifdef __linux__
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
                     "tiny.log(pcall(require, 'loop'))\n"
                     "tiny.log(pcall(require, 'broken'))\n"
                     "tiny.log(pcall(require, 'broken'))\n");
    std::ofstream(tree.path / "mods" / "util.lua")
            << "return {twice = function(x) return x * 2 end}";
    std::filesystem::create_directories(tree.path / "mods" / "lib");
    std::ofstream(tree.path / "mods" / "lib" / "deep.lua")
            << "return 'deep ' .. ...";
    std::ofstream(tree.path / "mods" / "loop.lua") << "require('loop')";
    std::ofstream(tree.path / "mods" / "broken.lua")
            << "error('broken module')";
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(logged("plugin mods: 8 true"));
    EXPECT_TRUE(logged("plugin mods: false the name of a module"));
    EXPECT_TRUE(logged("plugin mods: deep lib.deep"));
    EXPECT_TRUE(logged("requires itself"));
    // a module that fails fails the same way every time, not as a loop
    EXPECT_EQ(std::count_if(lines.begin(), lines.end(),
                            [](const std::string& line) {
                                return line.find("broken module") !=
                                       std::string::npos;
                            }),
              2);
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

TEST_F(HostTest, a_widget_with_an_icon_is_wider_and_can_change_it)
{
    tree.add("icons",
             "a = tiny.panel.label{text = '1', sample = '1'}\n"
             "b = tiny.panel.label{text = '1', sample = '1', icon = 'x', "
             "icon_under = 'y'}\n"
             "b:set_icon('z')\n"
             "b:set_icon(nil)\n"
             "tiny.log('widths', a ~= nil, b ~= nil)\n");
    const auto before = children_of(panel.get_window());
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(logged("plugin icons: widths true true"));
    const auto after = children_of(panel.get_window());
    ASSERT_EQ(after.size(), before.size() + 2);
    XWindowAttributes plain;
    XWindowAttributes iconic;
    XGetWindowAttributes(tiny::get_display(), after[after.size() - 2], &plain);
    XGetWindowAttributes(tiny::get_display(), after[after.size() - 1], &iconic);
    EXPECT_GT(iconic.width, plain.width);
}

TEST_F(HostTest, an_icon_can_only_be_set_where_the_widget_has_room_for_it)
{
    tree.add("room", "tiny.panel.label{text = 'x'}:set_icon('a')");
    tree.add("empty", "tiny.panel.label{text = 'x', icon = ''}:set_icon('a')");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_NE(host.error_of("room").find("without an icon"), std::string::npos);
    EXPECT_TRUE(host.error_of("empty").empty());
}

TEST_F(HostTest, a_window_stays_inside_of_the_panel_under_its_widget)
{
    tree.add("place",
             "b = tiny.panel.button{text = 'go', on_click = function()\n"
             "  tiny.log('before', b:popup_open())\n"
             "  b:popup{title = 'T', value = '9'}\n"
             "  tiny.log('after', b:popup_open())\n"
             "end}\n");
    const auto before = children_of(panel.get_window());
    PluginHost host(services);
    host.apply(tree.find(), config);
    const auto after = children_of(panel.get_window());
    ASSERT_EQ(after.size(), before.size() + 1);
    XWindowAttributes panel_at;
    XGetWindowAttributes(tiny::get_display(), panel.get_window(), &panel_at);
    send(after.back(), make_event(EnterNotify));
    send(after.back(), make_event(ButtonRelease));
    EXPECT_TRUE(logged("plugin place: before false"));
    EXPECT_TRUE(logged("plugin place: after true"));
    // the window of the plugin is a child of the root: the newest one
    const auto roots = children_of(root_window());
    ASSERT_FALSE(roots.empty());
    bool found = false;
    for (const ::Window w : roots) {
        XWindowAttributes at;
        XGetWindowAttributes(tiny::get_display(), w, &at);
        if (at.map_state == IsViewable && at.override_redirect != 0 &&
            at.width >= 240 && at.width < panel_at.width &&
            at.height < panel_at.width) {
            EXPECT_GE(at.x, panel_at.x);
            EXPECT_LE(at.x + at.width, panel_at.x + panel_at.width);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(HostTest, a_plugin_can_ask_whether_an_icon_exists)
{
    tree.add("ask",
             "tiny.log('has', tiny.has_icon('here'), tiny.has_icon('gone'),\n"
             "         tiny.has_icon(''))\n");
    {
        const IconFiles files({"here"});
        tiny::wm::SymbolicIcons icons(tiny::get_display(), root_window(),
                                      {files.dir()});
        services.icons = &icons;
        PluginHost host(services);
        host.apply(tree.find(), config);
        EXPECT_TRUE(logged("plugin ask: has true false false"));
        host.stop();
        services.icons = nullptr;
    }
    lines.clear();
    PluginHost without(services); // no icons at all
    without.apply(tree.find(), config);
    EXPECT_TRUE(logged("plugin ask: has false false false"));
}

TEST_F(HostTest, the_name_of_an_icon_is_a_text)
{
    tree.add("icon", "tiny.panel.label{text = 'x'}:set_icon(5)");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_NE(host.error_of("icon").find("icon is a text"), std::string::npos);
}

TEST_F(HostTest, a_window_opens_under_a_widget_and_is_closed_again)
{
    tree.add(
            "window",
            "b = tiny.panel.button{text = 'go', on_click = function()\n"
            "  b:popup{icon = 'x', title = 'T', subtitle = 'S', value = '9',\n"
            "    sections = {{heading = 'H', rows = {{icon = 'i', label = 'a', "
            "value = '1'}, {label = 'b'}}}, {rows = {}}}}\n"
            "  tiny.log('opened')\n"
            "  b:close_popup()\n"
            "  b:popup{title = 'again'}\n"
            "end}\n");
    const auto before = children_of(panel.get_window());
    PluginHost host(services);
    host.apply(tree.find(), config);
    const auto after = children_of(panel.get_window());
    ASSERT_EQ(after.size(), before.size() + 1);
    send(after.back(), make_event(EnterNotify));
    send(after.back(), make_event(ButtonRelease));
    EXPECT_TRUE(logged("opened"));
    EXPECT_EQ(host.running(), std::vector<std::string>{"window"});
    EXPECT_TRUE(host.error_of("window").empty());
}

TEST_F(HostTest, a_window_with_a_wrong_description_is_an_error_of_the_plugin)
{
    const std::vector<std::pair<std::string, std::string>> wrong = {
            {"b:popup(5)", "a table is expected"},
            {"b:popup{title = {}}", "title must be a text"},
            {"b:popup{sections = 3}", "sections must be a table"},
            {"b:popup{sections = {5}}", "a section must be a table"},
            {"b:popup{sections = {{rows = 5}}}", "rows must be a table"},
            {"b:popup{sections = {{rows = {5}}}}", "a row must be a table"},
            {"b:popup{sections = {{rows = {{label = {}}}}}}",
             "label must be a text"},
    };
    for (const auto& [code, message] : wrong) {
        PluginTree own;
        own.add("bad", "b = tiny.panel.button{text = 'x'}\n" + code);
        PluginHost host(services);
        host.apply(own.find(), config);
        EXPECT_NE(host.error_of("bad").find(message), std::string::npos)
                << code << ": " << host.error_of("bad");
    }
}

namespace {

//! The newest window of a plugin that is open (a child of the root)
::Window open_window_of_plugin()
{
    const auto roots = children_of(root_window());
    for (auto it = roots.rbegin(); it != roots.rend(); ++it) {
        XWindowAttributes at;
        XGetWindowAttributes(tiny::get_display(), *it, &at);
        if (at.map_state == IsViewable && at.override_redirect != 0 &&
            at.width >= 240) {
            return *it;
        }
    }
    return None;
}

} // namespace

TEST_F(HostTest, the_rows_of_a_window_call_the_functions_of_the_plugin)
{
    tree.add("rows",
             "b = tiny.panel.button{text = 'go', on_click = function()\n"
             "  b:popup{sections = {{heading = 'H', rows = {\n"
             "    {label = 'one', value = '1', on_click = function()\n"
             "      tiny.log('one clicked')\n"
             "    end},\n"
             "    {label = 'off', dim = true, on_click = function()\n"
             "      tiny.log('dim clicked')\n"
             "    end},\n"
             "    {label = 'Password', entry = true, secret = true,\n"
             "     id = 'pw', on_submit = function(text)\n"
             "      tiny.log('typed ' .. text)\n"
             "    end},\n"
             "  }}}}\n"
             "end}\n");
    const auto before = children_of(panel.get_window());
    PluginHost host(services);
    host.apply(tree.find(), config);
    const auto after = children_of(panel.get_window());
    ASSERT_EQ(after.size(), before.size() + 1);
    send(after.back(), make_event(EnterNotify));
    send(after.back(), make_event(ButtonRelease));
    const ::Window window = open_window_of_plugin();
    ASSERT_NE(window, None);

    type_text(window, "s3");
    type_key(window, XK_Return); // the field has the keys at first
    EXPECT_TRUE(logged("plugin rows: typed s3"));
    type_key(window, XK_Up); // from the field to the first row
    type_key(window, XK_Return);
    EXPECT_TRUE(logged("one clicked"));
    EXPECT_FALSE(logged("dim clicked"));
    EXPECT_TRUE(host.error_of("rows").empty());
}

TEST_F(HostTest, an_action_may_destroy_its_widget_and_show_the_window_again)
{
    tree.add("act", "b = tiny.panel.button{text = 'go', on_click = function()\n"
                    "  b:popup{sections = {{rows = {\n"
                    "    {label = 'again', on_click = function()\n"
                    "      b:popup{title = 'second', sections = {{rows = {\n"
                    "        {label = 'bye', on_click = function()\n"
                    "          b:destroy()\n"
                    "          tiny.log('destroyed')\n"
                    "        end}}}}}\n"
                    "    end}}}}}\n"
                    "end}\n");
    const auto before = children_of(panel.get_window());
    PluginHost host(services);
    host.apply(tree.find(), config);
    const auto after = children_of(panel.get_window());
    ASSERT_EQ(after.size(), before.size() + 1);
    send(after.back(), make_event(EnterNotify));
    send(after.back(), make_event(ButtonRelease));
    ::Window window = open_window_of_plugin();
    ASSERT_NE(window, None);
    type_key(window, XK_Down);
    type_key(window, XK_Return); // shows the window again
    window = open_window_of_plugin();
    ASSERT_NE(window, None);
    type_key(window, XK_Down);
    type_key(window, XK_Return); // destroys the widget under the window
    EXPECT_TRUE(logged("destroyed"));
    EXPECT_TRUE(host.error_of("act").empty());
    // the widget is deleted a moment later, not under its own function
    EXPECT_TRUE(loop.run_until([&] {
        return children_of(panel.get_window()).size() == before.size();
    }));
}

TEST_F(HostTest, a_row_with_a_wrong_action_or_flag_is_an_error_of_the_plugin)
{
    const std::vector<std::pair<std::string, std::string>> wrong = {
            {"{label = 'a', on_click = 5}", "on_click must be a function"},
            {"{label = 'a', on_submit = 'x'}", "on_submit must be a function"},
            {"{label = 'a', dim = 'yes'}", "dim must be true or false"},
            {"{label = 'a', entry = 1}", "entry must be true or false"},
            {"{label = 'a', secret = {}}", "secret must be true or false"},
            {"{label = 'a', id = {}}", "id must be a text"},
            {"{label = 'a', on_click = f, on_submit = f}", "not both"},
    };
    for (const auto& [row, message] : wrong) {
        PluginTree own;
        own.add("bad", "local f = function() end\n"
                       "b = tiny.panel.button{text = 'x'}\n"
                       "b:popup{sections = {{rows = {" +
                               row + "}}}}\n");
        PluginHost host(services);
        host.apply(own.find(), config);
        EXPECT_NE(host.error_of("bad").find(message), std::string::npos)
                << row << ": " << host.error_of("bad");
    }
}

TEST_F(HostTest, a_window_has_one_entry_at_most)
{
    tree.add("two",
             "b = tiny.panel.button{text = 'x'}\n"
             "local ok, e = pcall(b.popup, b, {sections = {{rows = {\n"
             "  {label = 'a', entry = true}, {label = 'b', entry = true}}}}})\n"
             "tiny.log(tostring(ok), e)\n");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(logged("false a window has one row with an entry at most"));
}

TEST_F(HostTest, a_window_has_a_limit_of_the_sections_and_rows)
{
    tree.add("many",
             "b = tiny.panel.button{text = 'x'}\n"
             "local rows = {}\n"
             "for i = 1, 41 do rows[i] = {label = 'r'} end\n"
             "local ok, e = pcall(b.popup, b, {sections = {{rows = rows}}})\n"
             "tiny.log(tostring(ok), e)\n"
             "local s = {}\n"
             "for i = 1, 9 do s[i] = {} end\n"
             "ok, e = pcall(b.popup, b, {sections = s})\n"
             "tiny.log(tostring(ok), e)\n");
    PluginHost host(services);
    host.apply(tree.find(), config);
    EXPECT_TRUE(logged("false too many rows in a section"));
    EXPECT_TRUE(logged("false too many sections"));
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
