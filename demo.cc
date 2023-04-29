#include "X11/Xatom.h"

#include "containers.h"
#include "buttons.h"
#include "wm_header.h"
#include "wm_buttons.h"
#include "x_util.h"

namespace tiny {
    extern Handlers handlers;
}

bool keep_running;

class Demo: public tiny::Container {
  public:
    Demo():
            tiny::Container(300, 200),
            vbox(tiny::Box::Type::Vertical, 300-2, 200-2),
            header(300-2),
            buttons(tiny::Box::Type::Horizontal, 300-4, 30),
            close_btn(80, 27, "Close")
    {
        realize(XDefaultRootWindow(display), 0, 0);

        char *title_utf8 = "Ďáblův advokát";
        char *title = "Dabluv advokat";

        header.set_title(title_utf8);
        XChangeProperty(display, window,
                        display.WM_NAME, XA_STRING,
                        8, PropModeReplace,
                        reinterpret_cast<unsigned char *>(title),
                        strlen(title));
        XChangeProperty(display, window,
                        display._NET_WM_NAME, display.UTF8_STRING,
                        8, PropModeReplace,
                        reinterpret_cast<unsigned char *>(title_utf8),
                        strlen(title_utf8));

        add(&vbox);
        vbox.push_start(&header);
        header.push_back(&x_btn, 6, 1);
        header.push_back(&o_btn, 12, 1);
        header.push_back(&__btn, 12, 1);

        vbox.push_back(&buttons);

        buttons.push_back(&close_btn);

        close_btn.on_click.connect(this,
                static_cast<tiny::object_signal_t>(&Demo::on_button_click));
        x_btn.on_click.connect(this,
                static_cast<tiny::object_signal_t>(&Demo::on_button_click));

        set_events();
        map_all();
    }

    void set_events(){
        Container::set_events();

        Atom wm_delete_window = display.WM_DELETE_WINDOW;
        XSetWMProtocols(display, window, &wm_delete_window, 1);

        connect(ClientMessage,
                static_cast<tiny::event_signal_t>(&Demo::on_client_message));
    }

    void on_client_message(const XEvent &e, void *)
    {
        if (e.xclient.message_type == display.WM_PROTOCOLS &&
            (Atom)e.xclient.data.l[0] == display.WM_DELETE_WINDOW)
        {
            printf("This is it!\n");
            keep_running = false;
        }
    }

    void on_button_click(Object * w, const XEvent &e, void *)
    {
        printf("on_button_click\n");
        keep_running = false;
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
    tiny::Box vbox;
    wm::Header header;
    wm::CloseButton x_btn;
    wm::MinimizeButton __btn;
    wm::MaximizeButton o_btn;
    tiny::Box buttons;

    tiny::LabelButton close_btn;
};

int main (int argc, char * argv[])
{
    tiny::Display::init();
    Demo demo;
    demo.main_loop();
    return 0;
}
