#include <string>

#include <gtest/gtest.h>

#include "object.h"

namespace tiny {
extern Handlers handlers;
}

// Records how it was called so tests can check the dispatching.
class Receiver : public tiny::Object {
  public:
    using tiny::Object::connect_window;
    using tiny::Object::disconnect_window;

    void on_event(const XEvent& e, void* data)
    {
        ++event_calls;
        last_event_type = e.type;
        last_data = data;
    }

    void on_signal(tiny::Object* owner, const XEvent& e, void* data)
    {
        ++signal_calls;
        last_owner = owner;
        last_event_type = e.type;
        last_data = data;
    }

    tiny::event_function_t event_slot()
    {
        return [this](const XEvent& e) { on_event(e, nullptr); };
    }

    tiny::Signal::Function signal_slot()
    {
        return [this](tiny::Object* o, const XEvent& e, void* d) {
            on_signal(o, e, d);
        };
    }

    int event_calls = 0;
    int signal_calls = 0;
    int last_event_type = 0;
    tiny::Object* last_owner = nullptr;
    void* last_data = nullptr;
};

static XEvent make_event(int type)
{
    XEvent e = {};
    e.type = type;
    return e;
}

TEST(HandlersTest, set_and_unset)
{
    tiny::Handlers handlers;
    Receiver r;

    handlers.set_handler(KeyPress, 42,
                         [&r](const XEvent& e) { r.on_event(e, nullptr); });
    EXPECT_EQ(handlers.count(std::make_pair<uint16_t, ::Window>(KeyPress, 42)),
              1u);

    handlers.unset_handler(KeyPress, 42);
    EXPECT_TRUE(handlers.empty());
}

TEST(HandlersTest, set_replaces_existing_handler)
{
    tiny::Handlers handlers;
    Receiver first;
    Receiver second;

    handlers.set_handler(KeyPress, 42, [&first](const XEvent& e) {
        first.on_event(e, nullptr);
    });
    handlers.set_handler(KeyPress, 42, [&second](const XEvent& e) {
        second.on_event(e, nullptr);
    });
    EXPECT_EQ(handlers.size(), 1u);

    handlers.call_handler({KeyPress, 42}, make_event(KeyPress));
    EXPECT_EQ(first.event_calls, 0);
    EXPECT_EQ(second.event_calls, 1);
}

TEST(HandlersTest, unset_unknown_handler_is_noop)
{
    tiny::Handlers handlers;

    handlers.unset_handler(KeyPress, 42);
    EXPECT_TRUE(handlers.empty());
}

TEST(HandlersTest, call_passes_event_and_data)
{
    tiny::Handlers handlers;
    Receiver r;
    int payload = 0;

    handlers.set_handler(ButtonPress, 7, [&r, &payload](const XEvent& e) {
        r.on_event(e, &payload);
    });
    handlers.call_handler({ButtonPress, 7}, make_event(ButtonPress));

    EXPECT_EQ(r.event_calls, 1);
    EXPECT_EQ(r.last_event_type, ButtonPress);
    EXPECT_EQ(r.last_data, &payload);
}

TEST(HandlersTest, handlers_are_keyed_by_type_and_window)
{
    tiny::Handlers handlers;
    Receiver a;
    Receiver b;

    handlers.set_handler(KeyPress, 1,
                         [&a](const XEvent& e) { a.on_event(e, nullptr); });
    handlers.set_handler(KeyPress, 2,
                         [&b](const XEvent& e) { b.on_event(e, nullptr); });
    handlers.call_handler({KeyPress, 2}, make_event(KeyPress));

    EXPECT_EQ(a.event_calls, 0);
    EXPECT_EQ(b.event_calls, 1);
}

TEST(SignalTest, unconnected_signal_is_false_and_noop)
{
    tiny::Signal signal;
    Receiver owner;

    EXPECT_FALSE(signal);
    signal(&owner, make_event(ButtonPress)); // must not crash
}

TEST(SignalTest, connected_signal_calls_the_function)
{
    tiny::Signal signal;
    Receiver r;
    Receiver owner;

    signal.connect(r.signal_slot());
    EXPECT_TRUE(signal);

    signal(&owner, make_event(ButtonPress));

    EXPECT_EQ(r.signal_calls, 1);
    EXPECT_EQ(r.last_owner, &owner);
    EXPECT_EQ(r.last_event_type, ButtonPress);
    EXPECT_EQ(r.last_data, nullptr);
}

TEST(SignalTest, connected_function_is_called_with_the_event)
{
    tiny::Signal signal;
    Receiver owner;
    int type = 0;

    signal.connect([&type](const XEvent& e) { type = e.type; });
    EXPECT_TRUE(signal);

    signal(&owner, make_event(KeyPress));

    EXPECT_EQ(type, KeyPress);
}

TEST(SignalTest, connecting_again_replaces_the_function)
{
    tiny::Signal signal;
    Receiver r;
    Receiver owner;
    int calls = 0;

    signal.connect([&calls](const XEvent&) { ++calls; });
    signal.connect(r.signal_slot());
    signal(&owner, make_event(ButtonPress));

    EXPECT_EQ(calls, 0);
    EXPECT_EQ(r.signal_calls, 1);
}

TEST(SignalTest, the_data_of_the_call_is_passed)
{
    tiny::Signal signal;
    Receiver r;
    int data = 0;

    signal.connect(r.signal_slot());
    signal(&r, make_event(ButtonPress), &data);
    EXPECT_EQ(r.last_data, &data);

    signal(&r, make_event(ButtonPress));
    EXPECT_EQ(r.last_data, nullptr);
}

TEST(SignalTest, reconnect_replaces_target)
{
    tiny::Signal signal;
    Receiver first;
    Receiver second;

    signal.connect(first.signal_slot());
    signal.connect(second.signal_slot());
    signal(&first, make_event(ButtonPress));

    EXPECT_EQ(first.signal_calls, 0);
    EXPECT_EQ(second.signal_calls, 1);
}

TEST(ObjectTest, connect_window_registers_global_handler)
{
    Receiver r;

    r.connect_window(KeyPress, 1001, r.event_slot());
    EXPECT_EQ(tiny::handlers.count({KeyPress, 1001}), 1u);

    tiny::handlers.call_handler({KeyPress, 1001}, make_event(KeyPress));
    EXPECT_EQ(r.event_calls, 1);

    r.disconnect_window(KeyPress, 1001);
    EXPECT_EQ(tiny::handlers.count({KeyPress, 1001}), 0u);
}

TEST(ObjectTest, destructor_disconnects_all_windows)
{
    {
        Receiver r;
        r.connect_window(KeyPress, 1002, r.event_slot());
        r.connect_window(KeyRelease, 1002, r.event_slot());
        r.connect_window(KeyPress, 1003, r.event_slot());
        EXPECT_EQ(tiny::handlers.count({KeyPress, 1002}), 1u);
    }

    EXPECT_EQ(tiny::handlers.count({KeyPress, 1002}), 0u);
    EXPECT_EQ(tiny::handlers.count({KeyRelease, 1002}), 0u);
    EXPECT_EQ(tiny::handlers.count({KeyPress, 1003}), 0u);
}

TEST(ObjectTest, delete_through_base_pointer_disconnects_windows)
{
    auto* r = new Receiver;
    r->connect_window(KeyPress, 1004, r->event_slot());
    const tiny::Object* base = r;

    delete base;

    EXPECT_EQ(tiny::handlers.count({KeyPress, 1004}), 0u);
}

TEST(ObjectTest, plain_object_can_be_deleted)
{
    auto* o = new tiny::Object;

    delete o;
}

TEST(LogTest, error_prints_formatted_message_to_stderr)
{
    ::testing::internal::CaptureStderr();
    tiny::error("value %d and %s", 42, "text");
    const std::string out = ::testing::internal::GetCapturedStderr();

    EXPECT_NE(out.find("value 42 and text"), std::string::npos);
}

TEST(LogTest, log_prints_function_and_message)
{
    ::testing::internal::CaptureStderr();
    tiny::log(__FILE__, __func__, __LINE__, "hello %d", 1);
    const std::string out = ::testing::internal::GetCapturedStderr();

    EXPECT_NE(out.find("hello 1"), std::string::npos);
    EXPECT_NE(out.find("TestBody"), std::string::npos);
}

TEST(LogTest, the_log_macro_prints_only_in_a_debug_build)
{
    ::testing::internal::CaptureStderr();
    TINY_LOG("macro %d", 2);
    const std::string out = ::testing::internal::GetCapturedStderr();

#ifdef TINY_DEBUG
    EXPECT_NE(out.find("macro 2"), std::string::npos);
#else
    EXPECT_TRUE(out.empty());
#endif
}

TEST(HandlersTest, a_missing_handler_is_ignored_and_not_created)
{
    tiny::Handlers handlers;

    handlers.call_handler({KeyPress, 42}, make_event(KeyPress));
    EXPECT_TRUE(handlers.empty());
}

TEST(HandlersTest, a_handler_can_remove_itself)
{
    tiny::Handlers handlers;
    int calls = 0;

    handlers.set_handler(KeyPress, 42, [&](const XEvent&) {
        ++calls;
        handlers.unset_handler(KeyPress, 42);
    });
    handlers.call_handler({KeyPress, 42}, make_event(KeyPress));

    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(handlers.empty());
}

TEST(SignalTest, function_gets_the_owner_the_event_and_the_data)
{
    tiny::Signal signal;
    int payload = 0;
    tiny::Object owner;
    tiny::Object* got_owner = nullptr;
    void* got_data = nullptr;
    int got_type = 0;

    signal.connect([&](tiny::Object* o, const XEvent& e, void* d) {
        got_owner = o;
        got_type = e.type;
        got_data = d;
    });
    EXPECT_TRUE(signal);
    signal(&owner, make_event(KeyPress), &payload);

    EXPECT_EQ(got_owner, &owner);
    EXPECT_EQ(got_type, KeyPress);
    EXPECT_EQ(got_data, &payload);
}

TEST(SignalTest, the_last_connection_wins_over_a_function_with_owner)
{
    tiny::Signal signal;
    Receiver r;
    int calls = 0;

    signal.connect([&](tiny::Object*, const XEvent&, void*) { ++calls; });
    signal.connect(r.signal_slot());
    signal(&r, make_event(KeyPress));

    EXPECT_EQ(calls, 0);
    EXPECT_EQ(r.signal_calls, 1);
}

// Two bases that are both an Object, like Client (Container and Window): the
// function knows its object, a cast pointer to a member does not.
namespace {
class FirstBase : public tiny::Object {};

class SecondBase : public tiny::Object {
  public:
    int calls = 0;
};

class Both : public FirstBase, public SecondBase {
  public:
    void listen(::Window w)
    {
        SecondBase::connect_window(KeyPress, w,
                                   [this](const XEvent&) { ++calls; });
    }
};
} // namespace

TEST(ObjectTest, a_lambda_works_in_the_class_with_two_object_bases)
{
    Both both;
    both.listen(77);

    tiny::handlers.call_handler({KeyPress, 77}, make_event(KeyPress));

    EXPECT_EQ(both.calls, 1);
}
