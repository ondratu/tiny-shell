#pragma once

#include <sys/types.h>

#include <string>
#include <vector>

namespace tiny {

/**
 * @short Start the program argv[0] with arguments in a new session.
 *
 * No shell is involved: argv[0] is searched in PATH and the arguments are
 * passed as they are. The program does not die with its parent and does not
 * share its terminal.
 *
 * A non-empty `directory` becomes the working directory of the program.
 * `environment` changes the environment of the program: "NAME=value" sets a
 * variable, "NAME" (no equal sign) removes it.
 *
 * Returns the pid of the child. When the program can't be started (empty
 * argv, fork, chdir or exec failure) returns -1 and sets errno.
 */
pid_t spawn(const std::vector<std::string>& argv,
            const std::string& directory = "",
            const std::vector<std::string>& environment = {});

/// Run a shell command line (pipes, variables, ~) with `/bin/sh -c`.
pid_t spawn_shell(const std::string& command);

/**
 * @short Run what the user typed: a program with arguments or a command line.
 *
 * A text of words only is started without a shell, so a program that does not
 * exist is reported (-1, errno ENOENT). A text with anything the shell
 * understands (quotes, pipes, variables, `~`, `*`, `VAR=value cmd`) goes to
 * `spawn_shell`, which can't tell, because the shell itself starts.
 */
pid_t spawn_command(const std::string& text);

/**
 * @short The program `name` that is next to the running program.
 *
 * When `argv0` has a directory (the build directory, an installation that is
 * not in PATH) it is the path of `name` there, if that file can be executed;
 * else just `name`, which is searched in PATH. The path is resolved, so a
 * change of the working directory does not break it.
 */
std::string sibling_program(const char* argv0, const std::string& name);

/// Collect finished children (SIGCHLD) so they do not stay as zombies.
void reap_children();

} // namespace tiny
