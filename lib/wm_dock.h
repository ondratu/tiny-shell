#pragma once

#include "containers.h"
#include "buttons.h"
#include "wm_window.h"

namespace wm {

class DockButton: public tiny::Widget {
    public:
        DockButton();
        virtual ~DockButton();

        inline virtual uint32_t get_border() const override
        { return 0; }

        virtual void set_events(long mask=0) override;

        inline void redraw(const XEvent& e)
        { on_expose(e, nullptr); }

        tiny::Signal on_click;

    protected:
        virtual void on_enter_notify(const XEvent& e, void* data);
        virtual void on_leave_notify(const XEvent& e, void* data);
        virtual void on_button_release(const XEvent& e, void *data);
        virtual void on_expose(const XEvent& e, void* data);
};

class Icon: public DockButton {
    public:
        Icon(Window* task);
        virtual ~Icon();

    protected:
        virtual void on_button_release(const XEvent& e, void* data) override;
        virtual void on_expose(const XEvent& e, void* data) override;

        Window* task;
};

class Dock: public tiny::Box {
    public:
        Dock();
        virtual ~Dock();

        void add(Window* window);
        void remove(Window* window);
        void redraw(Window* window, const XEvent& e);

        virtual void realize(::Window parent, int x, int y) override;
        virtual void resize(uint32_t width, uint32_t height) override;

        inline virtual const tiny::Style& get_style() const override
        { return tiny::theme.wm_dock; }

    private:
        DockButton icon;
        std::map<Window*, Icon*> wm_task_icons;
};

} // namespace
