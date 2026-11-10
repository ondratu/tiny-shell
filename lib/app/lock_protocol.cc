#include "app/lock_protocol.h"

#include <unistd.h>

#include <cerrno>
#include <csignal>

namespace tiny::app {

namespace {

struct Word {
    Kind kind;
    const char* name;
};

constexpr Word words[] = {
        {Kind::PromptHidden, "PROMPT_HIDDEN"},
        {Kind::PromptVisible, "PROMPT_VISIBLE"},
        {Kind::Info, "INFO"},
        {Kind::Error, "ERROR"},
        {Kind::Result, "RESULT"},
        {Kind::Answer, "ANSWER"},
        {Kind::Cancel, "CANCEL"},
};

} // namespace

std::string format_message(Kind kind, const std::string& text)
{
    std::string line;
    for (const Word& word : words) {
        if (word.kind == kind) {
            line = word.name;
        }
    }
    if (!text.empty()) {
        line += ' ';
        for (const char c : text) {
            line += (c == '\n' || c == '\r') ? ' ' : c;
        }
    }
    line += '\n';
    return line;
}

std::optional<Message> parse_message(const std::string& line)
{
    std::string clean = line;
    while (!clean.empty() && (clean.back() == '\n' || clean.back() == '\r')) {
        clean.pop_back();
    }
    for (const Word& word : words) {
        const std::string name = word.name;
        if (clean.compare(0, name.size(), name) != 0) {
            continue;
        }
        if (clean.size() == name.size()) {
            return Message{word.kind, ""};
        }
        if (clean[name.size()] == ' ') {
            return Message{word.kind, clean.substr(name.size() + 1)};
        }
    }
    return std::nullopt;
}

Message result_message(const Outcome& outcome)
{
    switch (outcome.verdict) {
        case Verdict::Right:
            return {Kind::Result, "OK"};
        case Verdict::Wrong:
            return {Kind::Result, "WRONG"};
        default:
            return {Kind::Result,
                    outcome.text.empty() ? "FAILED" : "FAILED " + outcome.text};
    }
}

Outcome outcome_of(const std::string& result_text)
{
    if (result_text == "OK") {
        return {Verdict::Right, ""};
    }
    if (result_text == "WRONG") {
        return {Verdict::Wrong, ""};
    }
    // anything else is not an unlock: a damaged line must not open the screen
    const std::string prefix = "FAILED";
    if (result_text.compare(0, prefix.size(), prefix) == 0) {
        return {Verdict::Failed, result_text.size() > prefix.size() + 1
                                         ? result_text.substr(prefix.size() + 1)
                                         : ""};
    }
    return {Verdict::Failed, "the answer is not understood"};
}

void LineBuffer::feed(const char* bytes, size_t size)
{
    pending.append(bytes, size);
}

std::optional<std::string> LineBuffer::next()
{
    for (;;) {
        const size_t end = pending.find('\n');
        if (end == std::string::npos) {
            if (pending.size() > max_line) { // it does not end
                wipe(pending);
                too_long = true;
            }
            return std::nullopt;
        }
        std::string line = pending.substr(0, end);
        pending.erase(0, end + 1);
        if (line.size() > max_line) {
            wipe(line);
            too_long = true;
            continue;
        }
        return line;
    }
}

void LineBuffer::clear()
{
    wipe(pending);
}

LineDialog::LineDialog(int input_fd, int output_fd):
    input(input_fd), output(output_fd)
{}

LineDialog::~LineDialog() = default;

bool LineDialog::write_line(const std::string& line) const
{
    const char* bytes = line.data();
    size_t left = line.size();
    while (left > 0) {
        const ssize_t written = write(output, bytes, left);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            return false;
        }
        bytes += written;
        left -= static_cast<size_t>(written);
    }
    return true;
}

bool LineDialog::ask(bool hidden, const std::string& prompt,
                     std::string& answer)
{
    if (!write_line(format_message(
                hidden ? Kind::PromptHidden : Kind::PromptVisible, prompt))) {
        return false;
    }
    for (;;) {
        while (auto line = buffer.next()) {
            auto message = parse_message(*line);
            wipe(*line);
            if (message && message->kind == Kind::Answer) {
                answer = std::move(message->text);
                return true;
            }
            if (message && message->kind == Kind::Cancel) {
                return false;
            }
            // anything else is not an answer: ignored
        }
        if (buffer.overflow()) {
            return false;
        }
        char bytes[512];
        const ssize_t got = read(input, bytes, sizeof bytes);
        if (got < 0 && errno == EINTR) {
            continue;
        }
        if (got <= 0) { // the lock is gone
            return false;
        }
        buffer.feed(bytes, static_cast<size_t>(got));
        wipe(bytes, static_cast<size_t>(got));
    }
}

void LineDialog::say(bool error, const std::string& text)
{
    write_line(format_message(error ? Kind::Error : Kind::Info, text));
}

void LineDialog::finish(const Outcome& outcome)
{
    const Message result = result_message(outcome);
    write_line(format_message(result.kind, result.text));
}

} // namespace tiny::app
