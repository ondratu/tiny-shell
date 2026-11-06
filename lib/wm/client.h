#pragma once

#include <memory>
#include <optional>
#include <utility>

#include "container.h"
#include "wm/header.h"
#include "wm/window_button.h"
#include "wm/edge.h"
#include "wm/size_label.h"
#include "wm/window.h"

namespace tiny::wm {

class Client : public tiny::Container, public Window {

  public:
    Client(::Window child, ::Window root, int width, int height,
           unsigned long functions, unsigned long decorations,
           const tiny::Rect& area);

    ~Client() override;

    static Window* create(::Window root, ::Window child,
                          const XWindowAttributes& attrs,
                          unsigned long functions, unsigned long decorations,
                          const tiny::Rect& area, bool fresh = true,
                          SymbolicIcons* icons = nullptr);

    //! The icons of the buttons of the title bar; they must live longer
    void set_icons(SymbolicIcons* icons)
    {
        cls_btn.set_icons(icons);
        min_btn.set_icons(icons);
        max_btn.set_icons(icons);
    }

    void realize(::Window parent, int x, int y) override;

    void set_events(long mask = 0) override;

    void map_all() override;

    void set_focus() override;

    ::Window frame() const override { return window; }

    void restyle_frame() override { Container::restyle(); }

    void raise() override;

    void lower() override;

    void stack_under(::Window sibling) override;

    //! The border of the frame, none without `DECOR_BORDER`
    int get_border() const override;

    //! The border, the header on the top
    FrameExtents frame_extents() const override;

    //! The frame of a client before it has one (`_NET_REQUEST_FRAME_EXTENTS`)
    static FrameExtents default_frame_extents();

    //! The frame the decoration bits ask for
    static FrameExtents frame_extents_for(unsigned long decorations);

    void move_to(int x, int y) override;

    //! Minimize (Iconify) window
    void minimize() override;

    void restore(int x = 0, int y = 0) override;

    void maximize() override;

    void fullscreen(bool on) override;

    void shade(bool on) override;

    //! Only a window with a title bar can be rolled up to it
    bool is_shadable() const override { return header_height() > 0; }

    //! Move and resize the frame, so the client gets the asked place
    void configure_request(const XConfigureRequestEvent& e) override;

    /* signal handlers */
    void on_close_click(tiny::Object* o, const XEvent& e, void* data);

    void on_minimize_click(tiny::Object* o, const XEvent& e, void* data);

    void on_maximize_click(tiny::Object* o, const XEvent& e, void* data);

    void on_resize(tiny::Object* o, const XEvent& e, void* data);

    void on_move_resize_begin(tiny::Object* o, const XEvent& e, void* data);

    void on_move_resize_motion(tiny::Object* o, const XEvent& e, void* data);

    void on_move_resize_end(tiny::Object* o, const XEvent& e, void* data);

    void on_window_drag_begin(tiny::Object* o, const XEvent& e,
                              void* data) override;

    void on_window_drag_motion(tiny::Object* o, const XEvent& e,
                               void* data) override;

    void on_window_drag_end(tiny::Object* o, const XEvent& e, void* data);

    void begin_drag(unsigned edges, int x_root, int y_root) override;

    void drag_to(int x_root, int y_root) override;

    void end_drag() override;

  protected:
    /* event handlers */

    void on_focus_in(const XEvent& e, void* data) override;

    void on_focus_out(const XEvent& e, void* data) override;

    void on_property_notify(const XEvent& e, void* data) override;

    //! Show the parts the hints allow, the others are hidden
    void on_motif_hints_changed(unsigned long new_decorations) override;

    //! The title with a note when the window does not answer
    std::string title_text() const;

    //! Mark the title of a window that does not answer
    void on_responding_changed() override;

    //! The height of the title bar, none without `DECOR_TITLE`
    int header_height() const;

    //! Show or hide the title bar and the handles of the decorations
    void apply_decorations();

    //! Place the visible buttons in a row from the right edge
    void layout_buttons();

    void place_maximized() override;

    void keep_in_work_area() override;

    //! Give the frame and the client this size (a resize of the edges `mask`)
    void resize_frame(const tiny::Rect& frame, uint16_t mask);

    void place_fullscreen() override;

    tiny::Rect frame_geometry() const override;

    //! Show the size of the client in units (e.g. columns and rows)
    void show_size_units(int client_width, int client_height);

    //! The buttons the window has: what Motif hints and the type allow
    bool has_close_button() const { return is_closable(); }
    bool has_minimize_button() const
    {
        return is_minimizable() && (decorations & MotifWMHints::DECOR_MINIMIZE);
    }
    bool has_maximize_button() const
    {
        return is_maximizable() && !is_auxiliary() &&
               (decorations & MotifWMHints::DECOR_MAXIMIZE);
    }

    unsigned long decorations = 0;
    int unshaded_height = 0;     //!< of the frame, while it is rolled up
    bool has_last_click = false; //!< on the title bar
    unsigned long last_click_time = 0;

    Header header;
    CloseButton cls_btn;
    MinimizeButton min_btn;
    MaximizeButton max_btn;
    BackWindow shadow;
    std::unique_ptr<SizeLabel> size_label; //!< only while the size is changed
    //! The newest size of a resize that waits for the client to draw the
    //! older one (`_NET_WM_SYNC_REQUEST`), and the edges that move
    std::optional<std::pair<tiny::Rect, uint16_t>> deferred_resize;
};

} // namespace tiny::wm
