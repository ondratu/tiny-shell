#include "fd_watch.h"

#include <utility>

namespace tiny {

nfds_t poll_count(const std::vector<pollfd>& fds)
{
    nfds_t count = 0;
    for (size_t i = 0; i < fds.size(); ++i) {
        ++count; // no conversion of the size, so no cast
    }
    return count;
}

FdWatch::Id FdWatch::add(int fd, std::function<void()> function)
{
    const Id id = next_id++;
    items[id] = {fd, std::move(function)};
    return id;
}

bool FdWatch::remove(Id id)
{
    return items.erase(id) > 0;
}

void FdWatch::prepare(std::vector<pollfd>& fds, std::vector<Id>& ids) const
{
    for (const auto& [id, item] : items) {
        fds.push_back({item.fd, POLLIN, 0});
        ids.push_back(id);
    }
}

int FdWatch::dispatch(const std::vector<pollfd>& fds,
                      const std::vector<Id>& ids, size_t first)
{
    int called = 0;
    for (size_t i = 0; i < ids.size() && first + i < fds.size(); ++i) {
        if ((fds[first + i].revents & (POLLIN | POLLHUP | POLLERR)) == 0) {
            continue;
        }
        const auto it = items.find(ids[i]);
        if (it == items.end()) {
            continue; // removed by a function called before
        }
        // a copy: the function may remove its own watch
        const std::function<void()> function = it->second.function;
        function();
        ++called;
    }
    return called;
}

} // namespace tiny
