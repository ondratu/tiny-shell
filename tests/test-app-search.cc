#include <gtest/gtest.h>

#include "app_search.h"

namespace {

using Indexes = std::vector<size_t>;

tiny::Application app(const std::string& id, const std::string& name,
                      const std::string& exec = "",
                      const std::string& generic_name = "",
                      const std::vector<std::string>& keywords = {})
{
    tiny::Application application;
    application.id = id;
    application.entry.name = name;
    application.entry.exec = exec;
    application.entry.generic_name = generic_name;
    application.entry.keywords = keywords;
    return application;
}

} // namespace

TEST(FoldText, lowers_ascii_and_keeps_the_rest)
{
    EXPECT_EQ(tiny::fold_text("Firefox-ESR 3.1_x"), "firefox-esr 3.1_x");
    EXPECT_EQ(tiny::fold_text(""), "");
}

TEST(FoldText, removes_the_accents)
{
    EXPECT_EQ(tiny::fold_text("Příliš žluťoučký kůň"), "prilis zlutoucky kun");
    EXPECT_EQ(tiny::fold_text("ĚŠČŘŽÝÁÍÉÚŮĎŤŇ"), "escrzyaieuudtn");
    EXPECT_EQ(tiny::fold_text("Ångström Ñandú Ørsted"),
              "angstrom nandu orsted");
    EXPECT_EQ(tiny::fold_text("Łódź Őrs Şan"), "lodz ors san");
    EXPECT_EQ(tiny::fold_text("Straße Æon"), "strasse aeon");
}

TEST(FoldText, removes_combining_accents)
{
    // "e" + combining caron, "a" + combining acute
    EXPECT_EQ(tiny::fold_text("e\xCC\x8C"
                              "a\xCC\x81"),
              "ea");
}

TEST(FoldText, ligatures_and_separators)
{
    EXPECT_EQ(tiny::fold_text("Œuvre Ĳssel æ ĳ"), "oeuvre ijssel ae ij");
    EXPECT_EQ(tiny::fold_text("a\xE2\x80\x93"
                              "b\xC2\xA0"
                              "c\xC2\xB7"
                              "d"),
              "a b c d");
}

TEST(FoldText, lowers_cyrillic_and_copies_other_letters)
{
    EXPECT_EQ(tiny::fold_text("ПРИВЕТ Ёж"), "привет ёж");
    EXPECT_EQ(tiny::fold_text("日本語 ×÷"), "日本語 ×÷");
}

TEST(FoldText, copies_broken_utf8)
{
    EXPECT_EQ(tiny::fold_text("A\xC3"), "a\xC3");
    EXPECT_EQ(tiny::fold_text("\xFF\x80Z"), "\xFF\x80z");
    EXPECT_EQ(tiny::fold_text("\xE2\x82"), "\xE2\x82");
    EXPECT_EQ(tiny::fold_text("\xC3\x28"), "\xC3(");
    // overlong forms and code points over U+10FFFF are copied as they are
    EXPECT_EQ(tiny::fold_text("\xE0\x80\x80"), "\xE0\x80\x80");
    EXPECT_EQ(tiny::fold_text("\xC0\x80"), "\xC0\x80");
    EXPECT_EQ(tiny::fold_text("\xF4\x90\x80\x80"), "\xF4\x90\x80\x80");
    EXPECT_EQ(tiny::fold_text("\xF5\x80\x80\x80"), "\xF5\x80\x80\x80");
    EXPECT_EQ(tiny::fold_text("\xF4\x8F\xBF\xBF"), "\xF4\x8F\xBF\xBF");
}

TEST(SearchApplications, matches_regardless_of_case_and_accents)
{
    const std::vector<tiny::Application> apps = {app("a", "Český slovník"),
                                                 app("b", "Calculator")};
    EXPECT_EQ(tiny::search_applications(apps, "cesky"), (Indexes{0}));
    EXPECT_EQ(tiny::search_applications(apps, "ČESKÝ"), (Indexes{0}));
    EXPECT_EQ(tiny::search_applications(apps, "CALC"), (Indexes{1}));
    EXPECT_TRUE(tiny::search_applications(apps, "zzz").empty());
}

TEST(SearchApplications, start_of_a_word_comes_before_a_substring)
{
    const std::vector<tiny::Application> apps = {
            app("a", "Firefox"), app("b", "Fire-fox"), app("c", "Black fox")};
    EXPECT_EQ(tiny::search_applications(apps, "fox"), (Indexes{1, 2, 0}));
}

TEST(SearchApplications, a_word_starts_after_any_sign)
{
    const std::vector<tiny::Application> apps = {app("a", "xterm"),
                                                 app("b", "My.term_x")};
    EXPECT_EQ(tiny::search_applications(apps, "term"), (Indexes{1, 0}));
}

TEST(SearchApplications, fields_are_ordered)
{
    const std::vector<tiny::Application> apps = {
            app("exec", "Zzz", "/usr/bin/editor"),
            app("keyword", "Yyy", "", "", {"text", "editor"}),
            app("generic", "Xxx", "", "Editor of text"), app("name", "Editor")};
    EXPECT_EQ(tiny::search_applications(apps, "editor"), (Indexes{3, 2, 1, 0}));
}

TEST(SearchApplications, a_better_match_comes_before_a_better_field)
{
    const std::vector<tiny::Application> apps = {
            app("a", "Moneditor", "editor"),
            app("b", "Zzz", "", "", {"editor"})};
    EXPECT_EQ(tiny::search_applications(apps, "editor"), (Indexes{1, 0}));
}

TEST(SearchApplications, exec_is_only_the_name_of_the_program)
{
    const std::vector<tiny::Application> apps = {
            app("a", "A", "wrapper FOO=bar \"/opt/my app/tool\" --new-window"),
            app("b", "B", "/opt/bin/tool --new-window"),
            app("c", "C", "\"/opt/my app/gedit\" %U")};
    EXPECT_EQ(tiny::search_applications(apps, "gedit"), (Indexes{2}));
    EXPECT_EQ(tiny::search_applications(apps, "tool"), (Indexes{1}));
    EXPECT_EQ(tiny::search_applications(apps, "wrapper"), (Indexes{0}));
    EXPECT_TRUE(tiny::search_applications(apps, "window").empty());
    EXPECT_TRUE(tiny::search_applications(apps, "opt").empty());
}

TEST(SearchApplications, every_word_must_match)
{
    const std::vector<tiny::Application> apps = {
            app("a", "Image Viewer", "", "", {"photo"}),
            app("b", "Image Editor"), app("c", "Photo Viewer")};
    EXPECT_EQ(tiny::search_applications(apps, "view image"), (Indexes{0}));
    EXPECT_EQ(tiny::search_applications(apps, "  image   view "), (Indexes{0}));
    EXPECT_EQ(tiny::search_applications(apps, "image photo"), (Indexes{0}));
    EXPECT_TRUE(tiny::search_applications(apps, "image nothing").empty());
}

TEST(SearchApplications, the_worst_word_decides_the_rank)
{
    const std::vector<tiny::Application> apps = {
            app("a", "Foobar tool"), app("b", "Xfoo tool"),
            app("c", "Bar", "", "Foo tool")};
    // a: both words start a word of the name, c: the generic name,
    // b: "foo" is inside a word
    EXPECT_EQ(tiny::search_applications(apps, "foo tool"), (Indexes{0, 2, 1}));
}

TEST(SearchApplications, recent_ones_come_first_among_equal_matches)
{
    const std::vector<tiny::Application> apps = {
            app("a", "Edit one"), app("b", "Edit two"), app("c", "Edit three"),
            app("d", "Reedit")};
    EXPECT_EQ(tiny::search_applications(apps, "edit"), (Indexes{0, 1, 2, 3}));
    EXPECT_EQ(tiny::search_applications(apps, "edit", {"c", "b"}),
              (Indexes{2, 1, 0, 3}));
    // a recent one does not overtake a better match
    EXPECT_EQ(tiny::search_applications(apps, "edit", {"d"}),
              (Indexes{0, 1, 2, 3}));
    // an unknown id changes nothing
    EXPECT_EQ(tiny::search_applications(apps, "edit", {"x", "b"}),
              (Indexes{1, 0, 2, 3}));
}

TEST(SearchApplications, empty_query_finds_all_with_the_recent_first)
{
    const std::vector<tiny::Application> apps = {app("a", "A"), app("b", "B"),
                                                 app("c", "C")};
    EXPECT_EQ(tiny::search_applications(apps, ""), (Indexes{0, 1, 2}));
    EXPECT_EQ(tiny::search_applications(apps, " \t", {"c", "a"}),
              (Indexes{2, 0, 1}));
    EXPECT_TRUE(tiny::search_applications({}, "").empty());
}

TEST(SearchApplications, words_are_separated_by_any_blank_and_punctuation)
{
    const std::vector<tiny::Application> apps = {
            app("a", "Black fox"),
            app("b", "Fire\xE2\x80\x93"
                     "fox"),
            app("c", "Cafe \xC5\xBDlut\xC3\xBD"), app("d", "Firefox")};
    // the en dash separates words, "fox" in Firefox is a substring
    EXPECT_EQ(tiny::search_applications(apps, "fox"), (Indexes{0, 1, 3}));
    // a trailing CR or a no-break space is not a part of the word
    EXPECT_EQ(tiny::search_applications(apps, "fox\r"), (Indexes{0, 1, 3}));
    EXPECT_EQ(tiny::search_applications(apps, "black\xC2\xA0"
                                              "fox"),
              (Indexes{0}));
    // Ž starts a word after the space
    EXPECT_EQ(tiny::search_applications(apps, "zlut"), (Indexes{2}));
}

TEST(SearchApplications, empty_exec_and_a_code_only_exec_find_nothing_by_exec)
{
    const std::vector<tiny::Application> apps = {app("a", "A", ""),
                                                 app("b", "B", "%U"),
                                                 app("c", "C", "\"unclosed")};
    EXPECT_TRUE(tiny::search_applications(apps, "u").empty());
    EXPECT_TRUE(tiny::search_applications(apps, "unclosed").empty());
    EXPECT_EQ(tiny::search_applications(apps, ""), (Indexes{0, 1, 2}));
}
