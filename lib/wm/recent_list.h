#pragma once

#include <algorithm>
#include <vector>

namespace tiny::wm {

/**
 * @short Items ordered by the time they were used: the latest first.
 *
 * An item is there once. It knows nothing about X, so the order of Alt + Tab
 * is decided (and tested) without a display.
 */
template <typename T>
class RecentList {
  public:
    //! The item was used now: it goes to the front (it is added when new)
    void touch(const T& item)
    {
        remove(item);
        list.insert(list.begin(), item);
    }

    //! A new item that was not used yet goes to the end; a known one stays
    void add_last(const T& item)
    {
        if (!contains(item)) {
            list.push_back(item);
        }
    }

    void remove(const T& item)
    {
        list.erase(std::remove(list.begin(), list.end(), item), list.end());
    }

    bool contains(const T& item) const
    {
        return std::find(list.begin(), list.end(), item) != list.end();
    }

    //! From the latest to the oldest
    const std::vector<T>& items() const { return list; }

  private:
    std::vector<T> list;
};

} // namespace tiny::wm
