#include "wm_edge.h"
#include "x_util.h"

namespace tiny {
    extern Handlers handlers;
}

bool keep_running;

class Demo: public wm::BackWindow {
  public:
    Demo():wm::BackWindow(300, 200)
    {
        display = XOpenDisplay(NULL);
        realize(display, DefaultRootWindow(display), 0, 0);
        // XSetWindowBackground(display, window, 0x000055);

        set_events();
        map_all();
    }

    ~Demo(){
        XCloseDisplay(display);
    }

    void set_events(){
        wm::BackWindow::set_events();

        Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", 0);
        XSetWMProtocols(display, window, &wm_delete_window, 1);

        connect(ClientMessage,
                static_cast<tiny::event_signal_t>(&Demo::on_client_message));
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

    void main_loop(){
        keep_running = true;

        XEvent event;
        while (keep_running) {
            XNextEvent(display, &event);
            auto signal_id = std::make_pair(event.type, event.xany.window);

            // call manager handlers first
            if (tiny::handlers.count(signal_id)){
                tiny::handlers.call_hanlder(signal_id, event);
                continue;
            }
            //printf("Unhandled event %s (%d)\n",
            //    event_to_string(event), event.type);
        }
    }
};

int main (int argc, char * argv[])
{
    Demo demo;
    demo.main_loop();
    return 0;
}
