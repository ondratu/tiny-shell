#include "buttons.h"

#include <memory>

int main (int argv, char * argc[])
{
    for (int i=0; i < 1000; ++i){
        auto item = std::shared_ptr<tiny::LabelButton>(
                new tiny::LabelButton(100, 28, "Menu Item"));
    }

    return 0;
}
