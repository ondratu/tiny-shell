#include "wm/system_info.h"

#include <sys/utsname.h>

#include <fstream>
#include <sstream>

#include "text.h"

namespace tiny::wm {

namespace {

std::string unquoted(std::string_view value)
{
    value = trim(value);
    if (value.size() < 2 || (value.front() != '"' && value.front() != '\'') ||
        value.back() != value.front()) {
        return std::string(value);
    }
    std::string text;
    for (size_t i = 1; i + 1 < value.size(); ++i) {
        if (value[i] == '\\' && i + 2 < value.size()) {
            ++i; // the next character is taken as it is
        }
        text += value[i];
    }
    return text;
}

std::string read_file(const char* path)
{
    std::ifstream in(path);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

} // namespace

std::string os_release_value(const std::string& text, const std::string& key)
{
    std::istringstream lines(text);
    std::string line;
    std::string found;
    while (std::getline(lines, line)) {
        const std::string_view view = trim(line);
        if (view.size() > key.size() && view.compare(0, key.size(), key) == 0 &&
            view[key.size()] == '=') {
            found = unquoted(view.substr(key.size() + 1)); // the last one wins
        }
    }
    return found;
}

std::string os_release_name(const std::string& text)
{
    std::string name = os_release_value(text, "PRETTY_NAME");
    if (name.empty()) {
        name = os_release_value(text, "NAME");
        const std::string version = os_release_value(text, "VERSION");
        if (!name.empty() && !version.empty()) {
            name += " " + version;
        }
    }
    return name;
}

SystemInfo system_info()
{
    SystemInfo info;
    std::string text = read_file("/etc/os-release");
    if (text.empty()) {
        text = read_file("/usr/lib/os-release");
    }
    info.name = os_release_name(text);

    struct utsname name = {};
    if (uname(&name) == 0) {
        info.kernel = std::string(name.sysname) + " " + name.release;
        info.machine = name.machine;
        if (info.name.empty()) {
            info.name = name.sysname;
        }
    }
    return info;
}

} // namespace tiny::wm
