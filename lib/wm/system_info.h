#pragma once

#include <string>

namespace tiny::wm {

//! What the about dialog tells of the computer
struct SystemInfo {
    std::string name;    //!< the distribution: "Debian GNU/Linux 13 (trixie)"
    std::string kernel;  //!< the system and its release: "Linux 6.12.1"
    std::string machine; //!< the kind of the processor: "x86_64"
};

/**
 * @short The value of a key in the text of an `os-release` file.
 *
 * A line is `KEY=value`; the value may be in single or double quotes, in which
 * a backslash protects the next character. Empty when the key is not there.
 */
std::string os_release_value(const std::string& text, const std::string& key);

/**
 * @short The name of the system from the text of an `os-release` file:
 * `PRETTY_NAME`, or `NAME` with `VERSION` when there is none; empty when the
 * text has none of them.
 */
std::string os_release_name(const std::string& text);

/**
 * @short The system this runs on, from `/etc/os-release` (or
 * `/usr/lib/os-release`) and `uname()`.
 *
 * Without an `os-release` file (the BSDs have none) the name is the one of the
 * system from `uname()`.
 */
SystemInfo system_info();

} // namespace tiny::wm
