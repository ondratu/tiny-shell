#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <X11/Xlib.h>

#include "button.h"
#include "container.h"

namespace tiny::wm {

/**
 * @short The layouts of the keyboard that the X server switches between (the
 * groups of XKB), and the one in use.
 */
struct KeyboardGroups {
    std::vector<std::string> codes; //!< short, what the panel shows: `cz`
    std::vector<std::string> names; //!< long, for the menu: `Czech`
    size_t current = 0;

    size_t size() const { return codes.size(); }

    //! The code of the layout in use; empty without any
    std::string code() const;
};

/**
 * @short The groups from the layouts of the server (`cz,us`: the property
 * `_XKB_RULES_NAMES`) and the names of the groups.
 *
 * The codes are the layouts. Without them (the server has no such property)
 * the first two letters of the names are used, in lower case. A group without
 * a name is named by its code; `current` that is not a group is the first one.
 */
KeyboardGroups make_keyboard_groups(const std::string& layouts,
                                    const std::vector<std::string>& names,
                                    size_t current);

/**
 * @short The keyboard of the X server, through the XKB extension: the groups
 * and the one in use, the events when they change and the switch to another.
 */
class XkbGroups {
  public:
    explicit XkbGroups(::Display* display);

    //! The server has XKB (without it there is nothing to show)
    bool available() const { return event_base >= 0; }

    //! Ask for the events of a change of the group and of the layouts
    void select_events() const;

    //! Is the event one of those of `select_events()`?
    bool is_event(const XEvent& e) const;

    //! What the server has now
    KeyboardGroups read() const;

    //! Switch to the group, as the key that switches the layout does
    void lock(size_t group) const;

  private:
    ::Display* display;
    int event_base = -1;
};

/**
 * @short The menu of the layouts: an item for each, the one in use is marked.
 *
 * `on_choose` gets the number of the layout that was clicked; the menu is
 * hidden before.
 */
class KeyboardMenu : public tiny::Popover {
  public:
    explicit KeyboardMenu(const KeyboardGroups& groups);

    ~KeyboardMenu() override;

    void realize(::Window parent, int x = 0, int y = 0) override;

    std::function<void(size_t)> on_choose;

  private:
    std::vector<std::unique_ptr<tiny::LabelButton>> items;
};

/**
 * @short The code of the layout in use at the right end of the panel
 * (`cz`). A click opens the menu of the layouts when there are more than one.
 */
class KeyboardIndicator : public tiny::LabelButton {
  public:
    KeyboardIndicator();

    ~KeyboardIndicator() override;

    void set_events(long mask = 0) override;

    //! Show the layout in use; the menu is made again when the layouts differ
    void set_groups(const KeyboardGroups& groups);

    //! The layout that the user chose in the menu
    std::function<void(size_t)> on_choose;

  private:
    void open_menu(const XEvent& e);

    KeyboardGroups groups;
    std::unique_ptr<KeyboardMenu> menu;
};

} // namespace tiny::wm
