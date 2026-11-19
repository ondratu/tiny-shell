#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace tiny::plugin {

/**
 * @short A value of JSON, what a plugin gets from a web service.
 *
 * The numbers are doubles. An object keeps the keys sorted (the order of the
 * file is not kept) and the last of equal keys wins.
 */
struct Json {
    enum class Type { Null, Boolean, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0;
    std::string string;
    std::vector<Json> array;
    std::map<std::string, Json> object;
};

//! The deepest nesting of arrays and objects that is read
constexpr int json_max_depth = 64;

/**
 * @short Read a text of JSON (RFC 8259).
 *
 * The text is one value with blanks around it. The bytes of a string are kept
 * as they are (they are not checked to be UTF-8) and the escapes (`\uXXXX`
 * with surrogate pairs too) are decoded. Returns nothing,
 * and tells why in `error` (with the position), when the text is wrong,
 * too deep, or has anything after the value.
 */
std::optional<Json> parse_json(const std::string& text,
                               std::string* error = nullptr);

} // namespace tiny::plugin
