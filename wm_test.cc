#include <memory>

#include "wm_widget.h"
#include "wm_buttons.h"

extern WMHandlers wm_handlers;
bool keep_running = true;

class WMTest: public WMWidget {
  public:
    WMTest(Display * display, Window parent):
        WMWidget(display, parent, 0, 0, 300, 200, 1, 0xaaa, 0xffffff)
    {
        button = std::shared_ptr<WMButton>(
            new WMButton(display, window, 100, 100, 30, 30));
        button->on_click.connect(
            this,
            static_cast<object_signal_t>(&WMTest::on_button_click));
        button->map();
        button->set_events();

        set_events();
        map();
    }

    virtual ~WMTest()
    {}

    virtual void set_events(){
        XSelectInput(display, window, StructureNotifyMask);

        Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", 0);
        XSetWMProtocols(display, window, &wm_delete_window, 1);

        connect(ClientMessage,
                static_cast<event_signal_t>(&WMTest::on_client_message));
    }

    void on_client_message(const XEvent &e, void *)
    {
        if (e.xclient.message_type == XInternAtom(display, "WM_PROTOCOLS", 1) &&
            (Atom)e.xclient.data.l[0] == XInternAtom(display, "WM_DELETE_WINDOW", 1))
        {
            printf("This is it!\n");
            keep_running = false;
        }
    }

    void on_button_click(WMObject * btn, const XEvent &e, void * data)
    {
        printf("Jo bylo kliknuto :)\n");
    }

  private:
    std::shared_ptr<WMButton> button;
};


int main (int argc, char * argv[])
{
    Display* display = XOpenDisplay(NULL);
    WMTest test(display, DefaultRootWindow(display));

    XEvent event;

    while (keep_running) {
        XNextEvent(display, &event);
        auto signal_id = std::make_pair(event.type, event.xany.window);

        // call manager handlers first
        if (wm_handlers.count(signal_id)){
            wm_handlers.call_hanlder(signal_id, event);
            continue;
        }
    }

    XCloseDisplay(display);
    return 0;
}
