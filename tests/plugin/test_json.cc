#include <gtest/gtest.h>

#include <string>

#include "plugin/json.h"

using tiny::plugin::Json;
using tiny::plugin::parse_json;

namespace {

//! The value of a text that is right
Json parsed(const std::string& text)
{
    auto value = parse_json(text);
    EXPECT_TRUE(value.has_value()) << text;
    return std::move(value).value_or(Json{});
}

std::string error_of(const std::string& text)
{
    std::string error;
    EXPECT_FALSE(parse_json(text, &error).has_value()) << text;
    return error;
}

} // namespace

TEST(JsonTest, reads_the_simple_values)
{
    EXPECT_EQ(parsed("null").type, Json::Type::Null);
    EXPECT_TRUE(parsed(" true ").boolean);
    EXPECT_FALSE(parsed("false").boolean);
    EXPECT_EQ(parsed("false").type, Json::Type::Boolean);
    EXPECT_EQ(parsed("-12.5e1").number, -125);
    EXPECT_EQ(parsed("0").number, 0);
    EXPECT_EQ(parsed("1E2").number, 100);
    EXPECT_EQ(parsed(R"("text")").string, "text");
}

TEST(JsonTest, reads_arrays_and_objects)
{
    const Json value =
            parsed(R"({"a": [1, 2, {"b": null}], "c": {}, "d": [], "a": 7})");
    ASSERT_EQ(value.type, Json::Type::Object);
    EXPECT_EQ(value.object.at("a").number, 7); // the last one wins
    EXPECT_EQ(value.object.at("c").type, Json::Type::Object);
    EXPECT_TRUE(value.object.at("c").object.empty());
    EXPECT_TRUE(value.object.at("d").array.empty());
    const Json list = parsed(R"([1, 2, {"b": null}])");
    ASSERT_EQ(list.array.size(), 3u);
    EXPECT_EQ(list.array[2].object.at("b").type, Json::Type::Null);
}

TEST(JsonTest, decodes_the_escapes_of_a_string)
{
    EXPECT_EQ(parsed("\"a\\n\\t\\\"\\\\\\/\\b\\f\\r\"").string,
              "a\n\t\"\\/\b\f\r");
    EXPECT_EQ(parsed("\"\\u00e9\\u20ac\"").string, "\xC3\xA9\xE2\x82\xAC");
    EXPECT_EQ(parsed("\"\\ud83d\\ude00\"").string, "\xF0\x9F\x98\x80");
    EXPECT_EQ(parsed("\"\\u0041\"").string, "A");
    EXPECT_EQ(parsed("\"\xC3\xA9\"").string, "\xC3\xA9"); // as it is
}

TEST(JsonTest, tells_what_is_wrong_and_where)
{
    EXPECT_NE(error_of("").find("ends too soon"), std::string::npos);
    EXPECT_NE(error_of("[1,").find("ends too soon"), std::string::npos);
    EXPECT_NE(error_of("[1 2]").find("comma"), std::string::npos);
    EXPECT_NE(error_of("{\"a\" 1}").find("colon"), std::string::npos);
    EXPECT_NE(error_of("{1: 2}").find("name"), std::string::npos);
    EXPECT_NE(error_of("{\"a\": 1 \"b\": 2}").find("comma"), std::string::npos);
    EXPECT_NE(error_of("1 2").find("after the value"), std::string::npos);
    EXPECT_NE(error_of("trux").find("wrong word"), std::string::npos);
    EXPECT_NE(error_of("nul").find("wrong word"), std::string::npos);
    EXPECT_NE(error_of("falsy").find("wrong word"), std::string::npos);
    EXPECT_NE(error_of("-").find("value"), std::string::npos);
    EXPECT_NE(error_of("1.").find("digit"), std::string::npos);
    EXPECT_NE(error_of("1e").find("digit"), std::string::npos);
    EXPECT_NE(error_of("1e999").find("too big"), std::string::npos);
    EXPECT_NE(error_of("\"open").find("not closed"), std::string::npos);
    EXPECT_NE(error_of("\"a\nb\"").find("control"), std::string::npos);
    EXPECT_NE(error_of("\"\\x\"").find("escape"), std::string::npos);
    EXPECT_NE(error_of("\"\\u12\"").find("cut"), std::string::npos);
    EXPECT_NE(error_of("\"\\u12zz\"").find("hexadecimal"), std::string::npos);
    EXPECT_NE(error_of("\"\\ud83d\"").find("low surrogate"), std::string::npos);
    EXPECT_NE(error_of("\"\\ud83d\\u0041\"").find("low surrogate"),
              std::string::npos);
    EXPECT_NE(error_of("\"\\ude00\"").find("low surrogate"), std::string::npos);
    EXPECT_NE(error_of("[1, ?]").find("at 5"), std::string::npos);
    EXPECT_NE(error_of("\"abc\\").find("not closed"), std::string::npos);
}

TEST(JsonTest, a_leading_zero_is_not_a_number)
{
    EXPECT_NE(error_of("01").find("after the value"), std::string::npos);
}

TEST(JsonTest, the_nesting_is_limited)
{
    std::string deep;
    for (int i = 0; i < tiny::plugin::json_max_depth; ++i) {
        deep += '[';
    }
    for (int i = 0; i < tiny::plugin::json_max_depth; ++i) {
        deep += ']';
    }
    EXPECT_TRUE(parse_json(deep).has_value());
    EXPECT_NE(error_of("[" + deep + "]").find("too deep"), std::string::npos);
    std::string objects;
    for (int i = 0; i < 100; ++i) {
        objects += "{\"a\":";
    }
    EXPECT_NE(error_of(objects).find("too deep"), std::string::npos);
}
