#pragma once

#include <list>
#include <memory>

#include "wm_widget.h"
#include "wm_theme.h"

namespace tiny {

class Container: public WMVirtual {
  public:
    Container(Display * display, Window parent);

    virtual ~Container();

    virtual void map_all();

    virtual void add(WMWidget * widget);

  protected:
    std::list<std::shared_ptr<WMWidget>> children;
};

} // namespace tiny
