#include "helpers.h"

#include "container.h"
#include "widget.h"

// Changing the variant of the theme: shown widgets take the new colors with
// restyle(), the others when they are mapped.
namespace {

class Light {
  public:
    Light() { tiny::theme.set_variant(tiny::Variant::Light); }
    ~Light() { tiny::theme.set_variant(tiny::Variant::Dark); }
};

uint32_t pixel_of(const tiny::Widget& w)
{
    return Image(w.get_window(), 4, 4).at(1, 1);
}

} // namespace

TEST(RestyleTest, a_shown_widget_takes_the_new_colors)
{
    tiny::Widget w(40, 40);
    w.realize(root_window(), 0, 0);
    w.map();
    ASSERT_EQ(pixel_of(w), tiny::theme.widget.normal_bg);
    const uint32_t dark = tiny::theme.widget.normal_bg;

    Light light;
    w.restyle();
    XSync(tiny::get_display(), False);

    EXPECT_NE(dark, tiny::theme.widget.normal_bg);
    EXPECT_EQ(pixel_of(w), tiny::theme.widget.normal_bg);
}

TEST(RestyleTest, a_widget_that_is_not_shown_takes_them_when_it_is_mapped)
{
    tiny::Widget w(40, 40);
    w.realize(root_window(), 0, 0);

    Light light;
    w.map();

    EXPECT_EQ(pixel_of(w), tiny::theme.widget.normal_bg);
}

TEST(RestyleTest, a_widget_that_is_not_realized_ignores_it)
{
    tiny::Widget w(40, 40);

    w.restyle();

    EXPECT_FALSE(w.get_realized());
}

TEST(RestyleTest, a_container_restyles_its_children)
{
    tiny::Container box(100, 100);
    tiny::Widget child(40, 40);
    box.realize(root_window(), 0, 0);
    box.add(&child, 10, 10);
    box.map_all();

    Light light;
    box.restyle();
    XSync(tiny::get_display(), False);

    EXPECT_EQ(pixel_of(box), tiny::theme.widget.normal_bg);
    EXPECT_EQ(pixel_of(child), tiny::theme.widget.normal_bg);
}

TEST(RestyleTest, an_input_only_widget_has_no_colors_to_change)
{
    tiny::Widget w(tiny::Widget::Type::Input, 40, 40);
    w.realize(root_window(), 0, 0);
    w.map();

    Light light;
    w.restyle();
    w.unmap();
    w.map(); // would be a BadMatch for a window with colors set

    XSync(tiny::get_display(), False);
    SUCCEED();
}
