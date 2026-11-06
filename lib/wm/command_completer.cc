#include "wm/command_completer.h"

#include <algorithm>
#include <filesystem>

#include <unistd.h>

namespace fs = std::filesystem;

namespace tiny::wm {

std::vector<std::string> list_commands(const std::string& path)
{
    std::vector<std::string> names;
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find(':', start);
        if (end == std::string::npos) {
            end = path.size();
        }
        const std::string dir = path.substr(start, end - start);
        start = end + 1;
        if (dir.empty()) { // the current directory is not a place of programs
            continue;
        }
        std::error_code error;
        fs::directory_iterator it(dir, error);
        for (const fs::directory_iterator done; !error && it != done;
             it.increment(error)) {
            std::error_code status_error;
            if (it->is_regular_file(status_error) &&
                access(it->path().c_str(), X_OK) == 0) {
                names.push_back(it->path().filename().string());
            }
        }
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}

void CommandCompleter::set_commands(std::vector<std::string> list)
{
    std::sort(list.begin(), list.end());
    list.erase(std::unique(list.begin(), list.end()), list.end());
    commands = std::move(list);
    matches.clear();
    last.clear();
    index = -1;
}

std::string CommandCompleter::next(const std::string& text)
{
    if (!matches.empty() && text == last) { // Tab again: the next program
        if (matches.size() == 1) {
            return text; // completed already
        }
        index = (index + 1) % static_cast<long>(matches.size());
        last = matches[static_cast<size_t>(index)];
        return last;
    }

    // a new word
    matches.clear();
    last.clear();
    index = -1;
    if (text.empty() || text.find_first_of(" \t") != std::string::npos) {
        return text;
    }
    // the sorted names that start with the text are together
    auto it = std::lower_bound(commands.begin(), commands.end(), text);
    for (; it != commands.end() && it->compare(0, text.size(), text) == 0;
         ++it) {
        matches.push_back(*it);
    }
    if (matches.empty()) {
        return text;
    }
    if (matches.size() == 1) {
        last = matches[0] + ' ';
        return last;
    }
    size_t common = matches.front().size();
    for (const std::string& name : matches) {
        size_t same = 0;
        while (same < common && name[same] == matches.front()[same]) {
            ++same;
        }
        common = same;
    }
    if (common > text.size()) {
        last = matches.front().substr(0, common);
        return last;
    }
    index = 0; // nothing to add to the text: the first program
    last = matches.front();
    return last;
}

} // namespace tiny::wm
