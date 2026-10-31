#pragma once

#include <string>

#include "widget.h"

namespace tiny {

/**
 * @short Single line text field.
 *
 * The widget only edits the text and tells about it with signals: on_change
 * after every edit, on_activate on Return and on_cancel on Escape. The keys
 * that move in a list (Up, Down, Page Up, Page Down) are passed to on_key.
 * What the text means is up to the owner.
 */
/// Latin-1 bytes (what `XLookupString` gives without an input method) as UTF-8.
std::string latin1_to_utf8(const char* bytes, int len);

class Entry : public Widget {
  public:
    Entry(int width, int height);

    ~Entry() override;

    void realize(Window parent, int x, int y) override;

    void set_events(long mask = 0) override;

    const Style& get_style() const override { return theme.entry; }

    //! Give the keyboard focus to the entry
    void set_focus();

    void set_text(const std::string& text);

    const std::string& get_text() const { return text; }

    void clear();

    Signal on_change;
    Signal on_activate;
    Signal on_cancel;
    Signal on_key;

  protected:
    void on_key_press(const XEvent& e, void* data);
    void on_expose(const XEvent& e, void* data);
    void on_focus_in(const XEvent& e, void* data);
    void on_focus_out(const XEvent& e, void* data);

  private:
    //! The typed characters in UTF-8; sets the keysym of the key
    std::string lookup(const XKeyEvent& key, KeySym& keysym);
    void redraw();
    void set_focus_state(bool focus, const XEvent& e);

    std::string text;

    XIM xim;
    XIC xic;
};

} // namespace tiny
