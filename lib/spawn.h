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
 * Returns the pid of the child. When the program can't be started (empty
 * argv, fork or exec failure) returns -1 and sets errno.
 */
pid_t spawn(const std::vector<std::string>& argv);

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

/// Collect finished children (SIGCHLD) so they do not stay as zombies.
void reap_children();

} // namespace tiny
