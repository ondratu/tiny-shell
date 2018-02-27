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
        display = XOpenDisplay(NULL);
        realize(display, DefaultRootWindow(display), 0, 0);

        header.set_title("Ďáblův advokát");

        add(&vbox);
        vbox.push_start(&header);
        header.push_back(&x_btn, 6, WM_WIN_HEADER_PADDING-WM_BTN_BORDER);
        header.push_back(&o_btn, 12, WM_WIN_HEADER_PADDING-WM_BTN_BORDER);
        header.push_back(&__btn, 12, WM_WIN_HEADER_PADDING-WM_BTN_BORDER);

        vbox.push_back(&buttons);

        buttons.push_back(&close_btn);

        close_btn.on_click.connect(this,
                static_cast<tiny::object_signal_t>(&Demo::on_button_click));
        x_btn.on_click.connect(this,
                static_cast<tiny::object_signal_t>(&Demo::on_button_click));

        set_events();
        map_all();
    }

    ~Demo(){
        XCloseDisplay(display);
    }

    void set_events(){
        Container::set_events();

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
    Demo demo;
    demo.main_loop();
    return 0;
}
