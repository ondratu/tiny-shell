#pragma once

#include "wm_widget.h"

class WMPanel: public WMWidget {
  public:
    WMPanel(Display * display, Window root);

   ~WMPanel();
};
