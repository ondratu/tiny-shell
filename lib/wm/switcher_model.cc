#include "wm/switcher_model.h"

namespace tiny::wm {

SwitcherModel::SwitcherModel(const std::vector<std::string>& keys):
    groups(group_dock({}, keys))
{}

const std::vector<size_t>& SwitcherModel::app_windows() const
{
    static const std::vector<size_t> none;
    return groups.empty() ? none : groups[app].tasks;
}

void SwitcherModel::start_apps(bool backwards)
{
    level = Level::Apps;
    app = 0;
    window = 0;
    next_app(backwards);
}

void SwitcherModel::start_windows(bool backwards)
{
    level = Level::Apps;
    app = 0;
    window = 0;
    next_window(backwards);
}

void SwitcherModel::next_app(bool backwards)
{
    if (groups.empty()) {
        return;
    }
    const size_t count = groups.size();
    app = backwards ? (app + count - 1) % count : (app + 1) % count;
    window = 0;
}

void SwitcherModel::move(bool backwards)
{
    if (level == Level::Apps) {
        next_app(backwards);
        return;
    }
    const size_t count = app_windows().size();
    if (count > 0) {
        window =
                backwards ? (window + count - 1) % count : (window + 1) % count;
    }
}

void SwitcherModel::next_window(bool backwards)
{
    if (groups.empty()) {
        return;
    }
    if (level == Level::Apps) {
        level = Level::Windows;
        window = 0;
    }
    move(backwards);
}

void SwitcherModel::down()
{
    if (level == Level::Apps && !groups.empty()) {
        level = Level::Windows;
        window = 0;
    }
}

void SwitcherModel::up()
{
    if (level == Level::Windows) {
        level = Level::Apps;
        window = 0;
    }
}

std::optional<size_t> SwitcherModel::result() const
{
    if (groups.empty()) {
        return std::nullopt;
    }
    const size_t chosen = groups[app].tasks[window];
    if (chosen == 0) { // the current window
        return std::nullopt;
    }
    return chosen;
}

} // namespace tiny::wm
