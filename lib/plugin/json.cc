#include "plugin/json.h"

#include <cmath>
#include <cstdlib>

namespace tiny::plugin {

namespace {

class Parser {
  public:
    explicit Parser(const std::string& text_arg): text(text_arg) {}

    std::optional<Json> run(std::string* error_out)
    {
        Json value;
        skip_blanks();
        if (!read_value(value, 0)) {
            report(error_out);
            return std::nullopt;
        }
        skip_blanks();
        if (pos != text.size()) {
            fail("unexpected text after the value");
            report(error_out);
            return std::nullopt;
        }
        return value;
    }

  private:
    void report(std::string* error_out) const
    {
        if (error_out) {
            *error_out = error + " at " + std::to_string(error_pos + 1);
        }
    }

    bool fail(const char* what)
    {
        if (error.empty()) {
            error = what;
            error_pos = pos;
        }
        return false;
    }

    void skip_blanks()
    {
        while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' ||
                                     text[pos] == '\n' || text[pos] == '\r')) {
            ++pos;
        }
    }

    bool word(const char* literal)
    {
        size_t n = 0;
        while (literal[n] != '\0') {
            ++n;
        }
        if (text.compare(pos, n, literal) != 0) {
            return false;
        }
        pos += n;
        return true;
    }

    // the depth is limited by json_max_depth
    // NOLINTNEXTLINE(misc-no-recursion)
    bool read_value(Json& out, int depth)
    {
        if (pos >= text.size()) {
            return fail("the text ends too soon");
        }
        switch (text[pos]) {
            case '{':
                return read_object(out, depth);
            case '[':
                return read_array(out, depth);
            case '"':
                out.type = Json::Type::String;
                return read_string(out.string);
            case 't':
                out.type = Json::Type::Boolean;
                out.boolean = true;
                if (!word("true")) {
                    return fail("wrong word");
                }
                return true;
            case 'f':
                out.type = Json::Type::Boolean;
                out.boolean = false;
                if (!word("false")) {
                    return fail("wrong word");
                }
                return true;
            case 'n':
                out.type = Json::Type::Null;
                if (!word("null")) {
                    return fail("wrong word");
                }
                return true;
            default:
                return read_number(out);
        }
    }

    bool read_number(Json& out)
    {
        const size_t start = pos;
        if (pos < text.size() && text[pos] == '-') {
            ++pos;
        }
        const auto digits = [&] {
            const size_t from = pos;
            while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
                ++pos;
            }
            return pos - from;
        };
        if (pos < text.size() && text[pos] == '0') {
            ++pos; // no more digits of an integer part that starts with 0
        } else if (digits() == 0) {
            pos = start;
            return fail("a value is expected");
        }
        if (pos < text.size() && text[pos] == '.') {
            ++pos;
            if (digits() == 0) {
                return fail("a digit is expected after the point");
            }
        }
        if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E')) {
            ++pos;
            if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) {
                ++pos;
            }
            if (digits() == 0) {
                return fail("a digit is expected in the exponent");
            }
        }
        out.type = Json::Type::Number;
        out.number = strtod(text.substr(start, pos - start).c_str(), nullptr);
        if (!std::isfinite(out.number)) {
            return fail("the number is too big");
        }
        return true;
    }

    static void put_utf8(std::string& out, unsigned long code)
    {
        if (code < 0x80) {
            out += static_cast<char>(code);
        } else if (code < 0x800) {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else if (code < 0x10000) {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
    }

    bool read_hex4(unsigned long& code)
    {
        if (pos + 4 > text.size()) {
            return fail("the escape \\u is cut");
        }
        code = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text[pos++];
            code <<= 4;
            if (c >= '0' && c <= '9') {
                code |= static_cast<unsigned long>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                code |= static_cast<unsigned long>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                code |= static_cast<unsigned long>(c - 'A' + 10);
            } else {
                --pos;
                return fail("a hexadecimal digit is expected");
            }
        }
        return true;
    }

    bool read_escape(std::string& out)
    {
        const char c = text[pos++];
        switch (c) {
            case '"':
            case '\\':
            case '/':
                out += c;
                return true;
            case 'b':
                out += '\b';
                return true;
            case 'f':
                out += '\f';
                return true;
            case 'n':
                out += '\n';
                return true;
            case 'r':
                out += '\r';
                return true;
            case 't':
                out += '\t';
                return true;
            case 'u': {
                unsigned long code = 0;
                if (!read_hex4(code)) {
                    return false;
                }
                if (code >= 0xD800 && code < 0xDC00) { // a pair is expected
                    unsigned long low = 0;
                    if (text.compare(pos, 2, "\\u") != 0) {
                        return fail("a low surrogate is expected");
                    }
                    pos += 2;
                    if (!read_hex4(low)) {
                        return false;
                    }
                    if (low < 0xDC00 || low > 0xDFFF) {
                        return fail("a low surrogate is expected");
                    }
                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                } else if (code >= 0xDC00 && code < 0xE000) {
                    return fail("a low surrogate without a high one");
                }
                put_utf8(out, code);
                return true;
            }
            default:
                --pos;
                return fail("wrong escape in the string");
        }
    }

    bool read_string(std::string& out)
    {
        ++pos; // the quote
        out.clear();
        while (pos < text.size()) {
            const char c = text[pos++];
            if (c == '"') {
                return true;
            }
            if (c == '\\') {
                if (pos >= text.size()) {
                    break;
                }
                if (!read_escape(out)) {
                    return false;
                }
            } else if (static_cast<unsigned char>(c) < 0x20) {
                --pos;
                return fail("a control character in the string");
            } else {
                out += c;
            }
        }
        return fail("the string is not closed");
    }

    // the depth is limited by json_max_depth
    // NOLINTNEXTLINE(misc-no-recursion)
    bool read_array(Json& out, int depth)
    {
        if (depth >= json_max_depth) {
            return fail("too deep");
        }
        out.type = Json::Type::Array;
        ++pos;
        skip_blanks();
        if (pos < text.size() && text[pos] == ']') {
            ++pos;
            return true;
        }
        for (;;) {
            Json item;
            skip_blanks();
            if (!read_value(item, depth + 1)) {
                return false;
            }
            out.array.push_back(std::move(item));
            skip_blanks();
            if (pos < text.size() && text[pos] == ',') {
                ++pos;
            } else if (pos < text.size() && text[pos] == ']') {
                ++pos;
                return true;
            } else {
                return fail("a comma or ] is expected");
            }
        }
    }

    // the depth is limited by json_max_depth
    // NOLINTNEXTLINE(misc-no-recursion)
    bool read_object(Json& out, int depth)
    {
        if (depth >= json_max_depth) {
            return fail("too deep");
        }
        out.type = Json::Type::Object;
        ++pos;
        skip_blanks();
        if (pos < text.size() && text[pos] == '}') {
            ++pos;
            return true;
        }
        for (;;) {
            skip_blanks();
            if (pos >= text.size() || text[pos] != '"') {
                return fail("a name in quotes is expected");
            }
            std::string key;
            if (!read_string(key)) {
                return false;
            }
            skip_blanks();
            if (pos >= text.size() || text[pos] != ':') {
                return fail("a colon is expected");
            }
            ++pos;
            skip_blanks();
            Json item;
            if (!read_value(item, depth + 1)) {
                return false;
            }
            out.object[key] = std::move(item);
            skip_blanks();
            if (pos < text.size() && text[pos] == ',') {
                ++pos;
            } else if (pos < text.size() && text[pos] == '}') {
                ++pos;
                return true;
            } else {
                return fail("a comma or } is expected");
            }
        }
    }

    const std::string& text;
    size_t pos = 0;
    std::string error;
    size_t error_pos = 0;
};

} // namespace

std::optional<Json> parse_json(const std::string& text, std::string* error)
{
    return Parser(text).run(error);
}

} // namespace tiny::plugin
