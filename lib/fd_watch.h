#pragma once

#include <poll.h>

#include <cstddef>
#include <functional>
#include <map>
#include <vector>

namespace tiny {

/**
 * @short File descriptors of the event loop that are read when they are ready.
 *
 * The loop asks `prepare()` for the descriptors to add to its `poll()` and
 * gives the result of the `poll()` to `dispatch()`, the same way `Timers` is
 * used. The functions are called from the loop, so they may add and remove
 * watches, also their own.
 */
class FdWatch {
  public:
    using Id = unsigned long;

    /**
     * @short Call `function` when `fd` can be read (also at the end of the file
     * and on an error, so the function sees the EOF).
     *
     * The descriptor stays the property of the caller, which must `remove()`
     * the watch before it closes it. Returns the id for `remove()`.
     */
    Id add(int fd, std::function<void()> function);

    //! Stop watching; false if the id is not (or no more) watched
    bool remove(Id id);

    //! Append the descriptors to `fds`, `ids` gets the ids of the new items
    void prepare(std::vector<pollfd>& fds, std::vector<Id>& ids) const;

    /**
     * @short Call the functions of the descriptors that are ready.
     *
     * `fds` and `ids` are the arrays of `prepare()` after `poll()`, `first`
     * is the index in `fds` where the items of `prepare()` start. Returns how
     * many functions were called.
     */
    int dispatch(const std::vector<pollfd>& fds, const std::vector<Id>& ids,
                 size_t first);

    bool empty() const { return items.empty(); }

  private:
    struct Item {
        int fd;
        std::function<void()> function;
    };

    Id next_id = 1;
    std::map<Id, Item> items;
};

} // namespace tiny
