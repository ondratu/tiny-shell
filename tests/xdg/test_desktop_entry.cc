#include <gtest/gtest.h>

#include <unistd.h>

#include <fstream>
#include <string>
#include <vector>

#include "xdg/desktop_entry.h"

using tiny::xdg::DesktopEntry;
using tiny::xdg::parse_desktop_entry;
using tiny::xdg::split_exec;
using Args = std::vector<std::string>;

TEST(DesktopEntryParseTest, reads_the_basic_keys)
{
    const DesktopEntry e = parse_desktop_entry("# comment\n"
                                               "[Desktop Entry]\n"
                                               "Type=Application\n"
                                               "Name=Firefox\n"
                                               "GenericName=Web Browser\n"
                                               "Comment=Browse the web\n"
                                               "Exec=firefox %u\n"
                                               "TryExec=firefox\n"
                                               "Icon=firefox\n"
                                               "Path=/opt/firefox\n"
                                               "Terminal=false\n");

    EXPECT_EQ(e.type, "Application");
    EXPECT_EQ(e.name, "Firefox");
    EXPECT_EQ(e.generic_name, "Web Browser");
    EXPECT_EQ(e.comment, "Browse the web");
    EXPECT_EQ(e.exec, "firefox %u");
    EXPECT_EQ(e.try_exec, "firefox");
    EXPECT_EQ(e.icon, "firefox");
    EXPECT_EQ(e.path, "/opt/firefox");
    EXPECT_FALSE(e.terminal);
}

TEST(DesktopEntryParseTest, reads_booleans_and_lists)
{
    const DesktopEntry e =
            parse_desktop_entry("[Desktop Entry]\n"
                                "Terminal=true\nNoDisplay=true\nHidden=true\n"
                                "Categories=Network;WebBrowser;\n"
                                "Keywords=a\\;b;web;;\n");

    EXPECT_TRUE(e.terminal);
    EXPECT_TRUE(e.no_display);
    EXPECT_TRUE(e.hidden);
    EXPECT_EQ(e.categories, (Args{"Network", "WebBrowser"}));
    EXPECT_EQ(e.keywords, (Args{"a;b", "web"}));
}

TEST(DesktopEntryParseTest, anything_but_true_is_false)
{
    const DesktopEntry e = parse_desktop_entry(
            "[Desktop Entry]\nTerminal=1\nNoDisplay=True\n");

    EXPECT_FALSE(e.terminal);
    EXPECT_FALSE(e.no_display);
}

TEST(DesktopEntryParseTest, ignores_other_groups)
{
    const DesktopEntry e = parse_desktop_entry(
            "[Desktop Action new]\nName=New window\nExec=x\n"
            "[Desktop Entry]\nName=Main\n"
            "[Desktop Action other]\nName=Other\n");

    EXPECT_EQ(e.name, "Main");
    EXPECT_EQ(e.exec, "");
}

TEST(DesktopEntryParseTest, keys_before_the_group_are_ignored)
{
    const DesktopEntry e = parse_desktop_entry("Name=Early\n");

    EXPECT_EQ(e.name, "");
}

TEST(DesktopEntryParseTest, tolerates_blanks_crlf_and_broken_lines)
{
    const DesktopEntry e = parse_desktop_entry("[Desktop Entry]\r\n"
                                               "  Name = Spaced  \r\n"
                                               "no equal sign\r\n"
                                               "\r\n"
                                               "Comment=x");

    EXPECT_EQ(e.name, "Spaced");
    EXPECT_EQ(e.comment, "x");
}

TEST(DesktopEntryParseTest, skips_the_utf8_bom)
{
    EXPECT_EQ(
            parse_desktop_entry("\xEF\xBB\xBF[Desktop Entry]\nName=Bom\n").name,
            "Bom");
}

TEST(DesktopEntryParseTest, escaped_backslash_does_not_escape_the_separator)
{
    const DesktopEntry e =
            parse_desktop_entry("[Desktop Entry]\nCategories=A\\\\;B;C\\;D\n");

    EXPECT_EQ(e.categories, (Args{"A\\", "B", "C;D"}));
}

TEST(DesktopEntryParseTest, last_line_without_a_newline_is_read)
{
    EXPECT_EQ(parse_desktop_entry("[Desktop Entry]\nName=End").name, "End");
}

TEST(DesktopEntryParseTest, only_blanks_in_value_is_empty)
{
    EXPECT_EQ(parse_desktop_entry("[Desktop Entry]\nName=   \n").name, "");
}

TEST(DesktopEntryParseTest, first_value_of_a_key_is_kept_ordering_aside)
{
    const DesktopEntry e =
            parse_desktop_entry("[Desktop Entry]\nName=One\nName=Two\n");

    EXPECT_EQ(e.name, "One");
}

TEST(DesktopEntryParseTest, unescapes_string_values)
{
    const DesktopEntry e =
            parse_desktop_entry("[Desktop Entry]\n"
                                "Comment=a\\sb\\nc\\td\\re\\\\f\\x\\\n");

    EXPECT_EQ(e.comment, "a b\nc\td\re\\f\\x\\");
}

TEST(DesktopEntryLocaleTest, picks_the_best_match)
{
    const std::string text = "[Desktop Entry]\n"
                             "Name=Files\n"
                             "Name[cs]=Soubory\n"
                             "Name[cs_CZ]=Soubory CZ\n"
                             "Name[cs@mod]=Soubory mod\n"
                             "Name[cs_CZ@mod]=Soubory CZ mod\n"
                             "Name[de]=Dateien\n";

    EXPECT_EQ(parse_desktop_entry(text, "cs_CZ.UTF-8@mod").name,
              "Soubory CZ mod");
    EXPECT_EQ(parse_desktop_entry(text, "cs_CZ.UTF-8").name, "Soubory CZ");
    EXPECT_EQ(parse_desktop_entry(text, "cs@mod").name, "Soubory mod");
    EXPECT_EQ(parse_desktop_entry(text, "cs").name, "Soubory");
    EXPECT_EQ(parse_desktop_entry(text, "de_DE.UTF-8").name, "Dateien");
}

TEST(DesktopEntryLocaleTest, falls_back_to_the_key_without_locale)
{
    const std::string text = "[Desktop Entry]\nName[de]=Dateien\nName=Files\n";

    EXPECT_EQ(parse_desktop_entry(text, "fr_FR").name, "Files");
    EXPECT_EQ(parse_desktop_entry(text, "").name, "Files");
    EXPECT_EQ(parse_desktop_entry(text, "C").name, "Files");
}

TEST(DesktopEntryLocaleTest, better_match_wins_in_any_order)
{
    const std::string text =
            "[Desktop Entry]\nName[cs]=B\nName[cs_CZ]=A\nName=C\n";

    EXPECT_EQ(parse_desktop_entry(text, "cs_CZ").name, "A");
}

TEST(DesktopEntryLocaleTest, worse_match_after_a_better_one_is_ignored)
{
    const std::string text =
            "[Desktop Entry]\nName[cs_CZ]=A\nName[cs]=B\nName=C\n";

    EXPECT_EQ(parse_desktop_entry(text, "cs_CZ").name, "A");
}

TEST(DesktopEntryLocaleTest, unterminated_locale_bracket_is_ignored)
{
    const std::string text = "[Desktop Entry]\nName[cs=A\nName=B\n";

    EXPECT_EQ(parse_desktop_entry(text, "cs").name, "B");
}

TEST(DesktopEntryLocaleTest, encoding_in_the_key_is_ignored)
{
    const std::string text = "[Desktop Entry]\nName=A\nName[cs_CZ.UTF-8]=B\n"
                             "Name[de.ISO-8859-1@mod]=C\n";

    EXPECT_EQ(parse_desktop_entry(text, "cs_CZ").name, "B");
    EXPECT_EQ(parse_desktop_entry(text, "de@mod").name, "C");
}

TEST(DesktopEntryLocaleTest, specification_example_for_sr_YU_Latn)
{
    const std::string text =
            "[Desktop Entry]\nName=Foo\nName[sr]=A\nName[sr@Latn]=B\n"
            "Name[sr_YU]=C\nName[sr_YU@Latn]=D\n";

    EXPECT_EQ(parse_desktop_entry(text, "sr_YU@Latn").name, "D");
    EXPECT_EQ(parse_desktop_entry(text, "sr_YU@Latin").name, "C");
    EXPECT_EQ(parse_desktop_entry(text, "sr@Latn").name, "B");
    EXPECT_EQ(parse_desktop_entry(text, "sr_RS").name, "A");
    EXPECT_EQ(parse_desktop_entry(text, "en_US").name, "Foo");
}

TEST(DesktopEntryLocaleTest, only_the_localizable_keys_have_a_locale)
{
    const DesktopEntry e = parse_desktop_entry(
            "[Desktop Entry]\nExec=real\nExec[cs]=fake\nIcon=a\n"
            "Icon[cs]=b\n",
            "cs");

    EXPECT_EQ(e.exec, "real");
    EXPECT_EQ(e.icon, "b");
}

TEST(DesktopEntryLocaleTest, localizes_keywords_and_comment)
{
    const std::string text =
            "[Desktop Entry]\nKeywords=web;\nKeywords[cs]=web;prohlížeč;\n"
            "Comment[cs]=Ahoj\n";

    const DesktopEntry e = parse_desktop_entry(text, "cs_CZ.UTF-8");

    EXPECT_EQ(e.keywords, (Args{"web", "prohlížeč"}));
    EXPECT_EQ(e.comment, "Ahoj");
}

TEST(DesktopEntryVisibleTest, only_shown_applications_are_visible)
{
    DesktopEntry e;
    EXPECT_FALSE(e.visible()); // no type

    e.type = "Application";
    EXPECT_FALSE(e.visible()); // nothing to show, nothing to run
    e.name = "Foo";
    EXPECT_FALSE(e.visible());
    e.exec = "foo";
    EXPECT_TRUE(e.visible());

    e.no_display = true;
    EXPECT_FALSE(e.visible());
    e.no_display = false;

    e.hidden = true;
    EXPECT_FALSE(e.visible());
    e.hidden = false;

    e.type = "Link";
    EXPECT_FALSE(e.visible());
}

// Appendix A of the Desktop Entry Specification 1.5, as it is published
TEST(DesktopEntrySpecificationTest, example_file)
{
    const DesktopEntry e = parse_desktop_entry(
            "[Desktop Entry]\n"
            "Version=1.0\n"
            "Type=Application\n"
            "Name=Foo Viewer\n"
            "Comment=The best viewer for Foo objects available!\n"
            "TryExec=fooview\n"
            "Exec=fooview %F\n"
            "Icon=fooview\n"
            "MimeType=image/x-foo;\n"
            "Actions=Gallery;Create;\n"
            "\n"
            "[Desktop Action Gallery]\n"
            "Exec=fooview --gallery\n"
            "Name=Browse Gallery\n"
            "\n"
            "[Desktop Action Create]\n"
            "Exec=fooview --create-new\n"
            "Name=Create a new Foo!\n"
            "Icon=fooview-new\n");

    EXPECT_EQ(e.type, "Application");
    EXPECT_EQ(e.name, "Foo Viewer");
    EXPECT_EQ(e.comment, "The best viewer for Foo objects available!");
    EXPECT_EQ(e.try_exec, "fooview");
    EXPECT_EQ(e.exec, "fooview %F");
    EXPECT_EQ(e.icon, "fooview");
    EXPECT_TRUE(e.visible());
    EXPECT_EQ(split_exec(e.exec), (Args{"fooview", "%F"}));
}

// "a literal backslash in a quoted argument needs four backslashes in the file"
TEST(DesktopEntrySpecificationTest,
     exec_double_escaping_of_backslash_and_dollar)
{
    const DesktopEntry e =
            parse_desktop_entry("[Desktop Entry]\n"
                                "Exec=sh -c \"echo \\\\\\\\ \\\\$HOME\"\n");

    EXPECT_EQ(split_exec(e.exec), (Args{"sh", "-c", "echo \\ $HOME"}));
}

TEST(DesktopEntryReadTest, reads_a_file)
{
    char name[] = "/tmp/tiny-desktop-XXXXXX";
    const int fd = mkstemp(name);
    ASSERT_GE(fd, 0);
    close(fd);
    {
        std::ofstream(name) << "[Desktop Entry]\nName=File\n";
    }

    const auto e = tiny::xdg::read_desktop_entry(name);
    unlink(name);

    if (!e) {
        FAIL() << "the file was not read";
    }
    EXPECT_EQ(e->name, "File");
}

TEST(DesktopEntryReadTest, directory_gives_nothing)
{
    EXPECT_FALSE(tiny::xdg::read_desktop_entry("/tmp"));
}

TEST(DesktopEntryReadTest, reads_a_file_longer_than_the_buffer)
{
    char name[] = "/tmp/tiny-desktop-XXXXXX";
    const int fd = mkstemp(name);
    ASSERT_GE(fd, 0);
    close(fd);
    {
        std::ofstream out(name);
        out << "[Desktop Entry]\n";
        for (int i = 0; i < 1000; ++i) {
            out << "# padding of the file to be longer than one read\n";
        }
        out << "Name=End\n";
    }

    const auto e = tiny::xdg::read_desktop_entry(name);
    unlink(name);

    if (!e) {
        FAIL() << "the file was not read";
    }
    EXPECT_EQ(e->name, "End");
}

TEST(DesktopEntryReadTest, empty_file_is_an_empty_entry)
{
    EXPECT_TRUE(tiny::xdg::read_desktop_entry("/dev/null"));
}

TEST(DesktopEntryReadTest, missing_file_gives_nothing)
{
    EXPECT_FALSE(tiny::xdg::read_desktop_entry("/nonexistent/x.desktop"));
}

TEST(SplitExecTest, splits_by_blanks)
{
    EXPECT_EQ(split_exec("firefox  --new-window\t%u"),
              (Args{"firefox", "--new-window", "%u"}));
}

TEST(SplitExecTest, empty_gives_nothing)
{
    EXPECT_TRUE(split_exec("").empty());
    EXPECT_TRUE(split_exec("   ").empty());
}

TEST(SplitExecTest, quotes_keep_blanks_together)
{
    EXPECT_EQ(split_exec("\"/opt/My App/run\" -x"),
              (Args{"/opt/My App/run", "-x"}));
}

TEST(SplitExecTest, quote_inside_an_argument_joins_the_parts)
{
    EXPECT_EQ(split_exec("a\"b c\"d"), (Args{"ab cd"}));
}

TEST(SplitExecTest, empty_quotes_are_an_empty_argument)
{
    EXPECT_EQ(split_exec("a \"\" b"), (Args{"a", "", "b"}));
}

TEST(SplitExecTest, backslash_escapes_only_special_characters_in_quotes)
{
    EXPECT_EQ(split_exec(R"("a\"b\`c\$d\\e\f")"), (Args{"a\"b`c$d\\e\\f"}));
}

TEST(SplitExecTest, backslash_outside_quotes_is_kept)
{
    EXPECT_EQ(split_exec(R"(a\b)"), (Args{"a\\b"}));
}

TEST(SplitExecTest, backslash_at_the_end_of_quotes_is_kept)
{
    EXPECT_TRUE(split_exec("\"a\\").empty()); // not closed
}

TEST(SplitExecTest, unclosed_quote_gives_nothing)
{
    EXPECT_TRUE(split_exec("a \"b c").empty());
}
