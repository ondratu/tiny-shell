#pragma once

#include <memory>
#include <string>
#include <vector>

#include "wm/monitors.h"
#include "widget.h"

namespace tiny::wm {

/**
 * @short The number of a monitor, big, with its name and size below, in a box
 * in the corner of the monitor.
 *
 * It helps to tell the monitors apart while they are set up.
 */
class MonitorLabel : public tiny::Widget {
  public:
    MonitorLabel();

    ~MonitorLabel() override;

    void set_events(long mask = 0) override;

    int get_border() const override { return 0; }

    /// The texts of the label: the number and the small line below it
    void set_text(const std::string& number_arg, const std::string& detail_arg);

    const std::string& get_number() const { return number; }
    const std::string& get_detail() const { return detail; }

    /// Put the label in the top left corner of the area
    void place(const tiny::Rect& area);

  protected:
    void on_expose(const XEvent& e, void* data);

  private:
    std::string number;
    std::string detail;
};

/**
 * @short The labels of all the monitors, over all the windows.
 *
 * `show()` makes or changes the labels to fit the monitors, `hide()` removes
 * them. A label says the number of the monitor (from 1, the one that the
 * settings use), the name of its output and its size; the primary monitor says
 * so.
 */
class MonitorLabels {
  public:
    MonitorLabels();

    ~MonitorLabels();

    MonitorLabels(const MonitorLabels&) = delete;
    MonitorLabels& operator=(const MonitorLabels&) = delete;

    //! The text under the number of a monitor: its output, size, and whether
    //! it is the primary one
    static std::string detail_of(const Monitor& monitor, bool primary);

    /**
     * Show a label in the corner of the `areas` (the work areas of the
     * monitors, so that the label is not under a panel); an area that is
     * missing is the monitor.
     */
    void show(::Window root, const Monitors& monitors, size_t primary,
              const std::vector<tiny::Rect>& areas = {});

    void hide();

    bool is_shown() const { return !labels.empty(); }

    //! Over all the windows again
    void raise();

    const std::vector<std::unique_ptr<MonitorLabel>>& get_labels() const
    {
        return labels;
    }

  private:
    std::vector<std::unique_ptr<MonitorLabel>> labels;
};

} // namespace tiny::wm
