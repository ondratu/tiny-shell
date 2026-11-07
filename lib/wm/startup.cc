#include "wm/startup.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace tiny::wm {

std::optional<StartupMessage> parse_startup_message(const std::string& text)
{
    const size_t colon = text.find(':');
    if (colon == std::string::npos || colon == 0) {
        return std::nullopt;
    }
    StartupMessage message;
    message.command = text.substr(0, colon);
    size_t i = colon + 1;
    while (i < text.size()) {
        while (i < text.size() && text[i] == ' ') {
            ++i;
        }
        const size_t equal = text.find('=', i);
        if (i >= text.size() || equal == std::string::npos) {
            break;
        }
        const std::string key = text.substr(i, equal - i);
        i = equal + 1;
        std::string value;
        if (i < text.size() && text[i] == '"') { // \" and \\ are escaped
            for (++i; i < text.size() && text[i] != '"'; ++i) {
                if (text[i] == '\\' && i + 1 < text.size()) {
                    ++i;
                }
                value += text[i];
            }
            ++i; // the closing quote
        } else {
            for (; i < text.size() && text[i] != ' '; ++i) {
                value += text[i];
            }
        }
        if (key.find(' ') == std::string::npos) { // a stray word is no key
            message.values[key] = value;
        }
    }
    return message;
}

void StartupNotification::feed(::Window sender, const char* data, bool begin,
                               Clock::time_point now)
{
    // a client message has 20 bytes; the text ends with a zero byte
    const size_t length = strnlen(data, 20);
    if (begin) {
        if (partial.size() >= 16 && !partial.count(sender)) {
            partial.erase(partial.begin()); // senders that never finish
        }
        partial[sender].clear();
    }
    const auto it = partial.find(sender);
    if (it == partial.end()) {
        return; // the beginning was not seen
    }
    it->second.append(data, length);
    if (it->second.size() > longest_message) {
        partial.erase(it);
        return;
    }
    if (length < 20) { // the zero byte: the text is complete
        const std::string text = std::move(it->second);
        partial.erase(it);
        if (const auto message = parse_startup_message(text)) {
            handle(*message, now);
        }
    }
}

void StartupNotification::handle(const StartupMessage& message,
                                 Clock::time_point now)
{
    const auto value = [&message](const char* key) {
        const auto it = message.values.find(key);
        return it == message.values.end() ? std::string() : it->second;
    };
    const std::string id = value("ID");
    if (id.empty()) {
        return;
    }
    if (message.command == "remove") {
        finish(id);
    } else if (message.command == "new") {
        StartupSequence sequence;
        sequence.id = id;
        sequence.name = value("NAME");
        sequence.bin = value("BIN");
        sequence.icon = value("ICON");
        sequence.wmclass = value("WMCLASS");
        sequence.silent = value("SILENT") == "1";
        begin(std::move(sequence), now);
    } else if (message.command == "change") {
        const auto it = running.find(id);
        if (it == running.end()) {
            return;
        }
        for (const auto& [key, text] : message.values) {
            if (key == "NAME") {
                it->second.name = text;
            } else if (key == "BIN") {
                it->second.bin = text;
            } else if (key == "ICON") {
                it->second.icon = text;
            } else if (key == "WMCLASS") {
                it->second.wmclass = text;
            } else if (key == "SILENT") {
                it->second.silent = text == "1";
            }
        }
    }
}

void StartupNotification::begin(StartupSequence sequence, Clock::time_point now)
{
    if (running.size() >= most_sequences && !running.count(sequence.id)) {
        // the one that waits for the end of its time the shortest goes
        running.erase(std::min_element(running.begin(), running.end(),
                                       [](const auto& a, const auto& b) {
                                           return a.second.deadline <
                                                  b.second.deadline;
                                       }));
    }
    sequence.deadline = now + timeout;
    const std::string id = sequence.id;
    running[id] = std::move(sequence);
}

bool StartupNotification::finish(const std::string& id)
{
    return running.erase(id) > 0;
}

bool StartupNotification::finish_class(const std::string& instance,
                                       const std::string& klass)
{
    const auto oldest = [this, &instance, &klass] {
        auto found = running.end();
        for (auto it = running.begin(); it != running.end(); ++it) {
            const std::string& wanted = it->second.wmclass;
            if (!wanted.empty() && (wanted == instance || wanted == klass) &&
                (found == running.end() ||
                 it->second.deadline < found->second.deadline)) {
                found = it;
            }
        }
        return found;
    }();
    if (oldest == running.end()) {
        return false;
    }
    running.erase(oldest);
    return true;
}

bool StartupNotification::expire(Clock::time_point now)
{
    bool dropped = false;
    for (auto it = running.begin(); it != running.end();) {
        if (it->second.deadline <= now) {
            it = running.erase(it);
            dropped = true;
        } else {
            ++it;
        }
    }
    return dropped;
}

bool StartupNotification::busy() const
{
    return std::any_of(running.begin(), running.end(),
                       [](const auto& entry) { return !entry.second.silent; });
}

std::optional<StartupNotification::Clock::time_point>
StartupNotification::next_deadline() const
{
    std::optional<Clock::time_point> soonest;
    for (const auto& entry : running) {
        if (!soonest || entry.second.deadline < *soonest) {
            soonest = entry.second.deadline;
        }
    }
    return soonest;
}

} // namespace tiny::wm
