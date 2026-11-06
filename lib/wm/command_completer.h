#pragma once

#include <string>
#include <vector>

namespace tiny::wm {

/**
 * @short The names of the programs in the directories of a `PATH` (colons
 * between them): regular files that can be executed, sorted, every name once.
 * A directory that can't be read is skipped.
 */
std::vector<std::string> list_commands(const std::string& path);

/**
 * @short Tab in the field of the run dialog: completes the name of a program.
 *
 * The first Tab extends what is typed to what all the programs that start with
 * it have in common (a single program is completed and gets a blank after it,
 * so the arguments can follow). The next Tabs walk through the programs one by
 * one. Anything the user types or changes in between starts it again, because
 * the text is not the one `next()` gave.
 *
 * Only the first word of the line is completed: a text with a blank, and an
 * empty text, stay as they are.
 */
class CommandCompleter {
  public:
    //! The programs to choose from; need not be sorted
    void set_commands(std::vector<std::string> list);

    //! The text after Tab, `text` is the text now
    std::string next(const std::string& text);

  private:
    std::vector<std::string> commands; //!< sorted
    std::vector<std::string> matches;  //!< of `prefix`
    std::string last;                  //!< what `next()` returned
    long index = -1;                   //!< of `matches`, -1 before the first
};

} // namespace tiny::wm
