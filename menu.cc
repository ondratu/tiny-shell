#include "containers.h"
#include "buttons.h"
#include "x_util.h"

namespace tiny {
    extern Handlers handlers;
}

bool keep_running;

class Demo: public tiny::Box {
  public:
    Demo(): tiny::Box(tiny::Box::Type::Horizontal, 300, 30),
            main_button(100, 28, "Menu"),
            menu(150, 300)
    {
        display = XOpenDisplay(NULL);
        realize(display, DefaultRootWindow(display), 0, 0);

        menu.realize(display, DefaultRootWindow(display));
        push_start(&main_button);
        main_button.set_popover(&menu);


        for (int i=0; i < 8; ++i) {
            auto item = std::shared_ptr<tiny::LabelButton>(
                    new tiny::LabelButton(148, 28, "Menu Item"));
            btns.push_back(item);
        }

        for (auto it: btns) {
            menu.push_start(it.get());
        }

        set_events();
        map_all();
    }

    ~Demo(){
        XCloseDisplay(display);
    }

    void set_events(){
        tiny::Box::set_events();

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
            printf("Unhalded event: %s\n", event_to_string(event));
        }
    }

  private:
    tiny::MenuButton main_button;
    tiny::Popover menu;
    std::list<std::shared_ptr<tiny::LabelButton>> btns;
};

int main (int argc, char * argv[])
{
    Demo demo;
    demo.main_loop();
    return 0;
}
