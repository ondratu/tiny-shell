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

    tiny::event_signal_t event_slot()
    {
        return static_cast<tiny::event_signal_t>(&Receiver::on_event);
    }

    tiny::object_signal_t signal_slot()
    {
        return static_cast<tiny::object_signal_t>(&Receiver::on_signal);
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

TEST(Handlers, set_and_unset)
{
    tiny::Handlers handlers;
    Receiver r;

    handlers.set_handler(KeyPress, 42, &r, r.event_slot(), nullptr);
    EXPECT_EQ(handlers.count(std::make_pair<uint16_t, ::Window>(KeyPress, 42)),
              1u);

    handlers.unset_handler(KeyPress, 42);
    EXPECT_TRUE(handlers.empty());
}

TEST(Handlers, set_replaces_existing_handler)
{
    tiny::Handlers handlers;
    Receiver first, second;

    handlers.set_handler(KeyPress, 42, &first, first.event_slot(), nullptr);
    handlers.set_handler(KeyPress, 42, &second, second.event_slot(), nullptr);
    EXPECT_EQ(handlers.size(), 1u);

    handlers.call_hanlder({KeyPress, 42}, make_event(KeyPress));
    EXPECT_EQ(first.event_calls, 0);
    EXPECT_EQ(second.event_calls, 1);
}

TEST(Handlers, unset_unknown_handler_is_noop)
{
    tiny::Handlers handlers;

    handlers.unset_handler(KeyPress, 42);
    EXPECT_TRUE(handlers.empty());
}

TEST(Handlers, call_passes_event_and_data)
{
    tiny::Handlers handlers;
    Receiver r;
    int payload = 0;

    handlers.set_handler(ButtonPress, 7, &r, r.event_slot(), &payload);
    handlers.call_hanlder({ButtonPress, 7}, make_event(ButtonPress));

    EXPECT_EQ(r.event_calls, 1);
    EXPECT_EQ(r.last_event_type, ButtonPress);
    EXPECT_EQ(r.last_data, &payload);
}

TEST(Handlers, handlers_are_keyed_by_type_and_window)
{
    tiny::Handlers handlers;
    Receiver a, b;

    handlers.set_handler(KeyPress, 1, &a, a.event_slot(), nullptr);
    handlers.set_handler(KeyPress, 2, &b, b.event_slot(), nullptr);
    handlers.call_hanlder({KeyPress, 2}, make_event(KeyPress));

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

TEST(SignalTest, connected_signal_calls_method)
{
    tiny::Signal signal;
    Receiver r;
    Receiver owner;

    signal.connect(&r, r.signal_slot());
    EXPECT_TRUE(signal);

    signal(&owner, make_event(ButtonPress));

    EXPECT_EQ(r.signal_calls, 1);
    EXPECT_EQ(r.last_owner, &owner);
    EXPECT_EQ(r.last_event_type, ButtonPress);
    EXPECT_EQ(r.last_data, nullptr);
}

TEST(SignalTest, connect_data_is_default_and_call_data_overrides)
{
    tiny::Signal signal;
    Receiver r;
    int connected = 0, override_data = 0;

    signal.connect(&r, r.signal_slot(), &connected);

    signal(&r, make_event(ButtonPress));
    EXPECT_EQ(r.last_data, &connected);

    signal(&r, make_event(ButtonPress), &override_data);
    EXPECT_EQ(r.last_data, &override_data);
}

TEST(SignalTest, reconnect_replaces_target)
{
    tiny::Signal signal;
    Receiver first, second;

    signal.connect(&first, first.signal_slot());
    signal.connect(&second, second.signal_slot());
    signal(&first, make_event(ButtonPress));

    EXPECT_EQ(first.signal_calls, 0);
    EXPECT_EQ(second.signal_calls, 1);
}

TEST(ObjectTest, connect_window_registers_global_handler)
{
    Receiver r;

    r.connect_window(KeyPress, 1001, r.event_slot());
    EXPECT_EQ(tiny::handlers.count({KeyPress, 1001}), 1u);

    tiny::handlers.call_hanlder({KeyPress, 1001}, make_event(KeyPress));
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
    Receiver* r = new Receiver;
    r->connect_window(KeyPress, 1004, r->event_slot());
    tiny::Object* base = r;

    delete base;

    EXPECT_EQ(tiny::handlers.count({KeyPress, 1004}), 0u);
}

TEST(ObjectTest, plain_object_can_be_deleted)
{
    tiny::Object* o = new tiny::Object;

    delete o;
}

TEST(LogTest, error_prints_formatted_message_to_stderr)
{
    ::testing::internal::CaptureStderr();
    tiny::error("value %d and %s", 42, "text");
    std::string out = ::testing::internal::GetCapturedStderr();

    EXPECT_NE(out.find("value 42 and text"), std::string::npos);
}

TEST(LogTest, log_prints_function_and_message)
{
    ::testing::internal::CaptureStderr();
    TINY_LOG("hello %d", 1);
    std::string out = ::testing::internal::GetCapturedStderr();

    EXPECT_NE(out.find("hello 1"), std::string::npos);
    EXPECT_NE(out.find("TestBody"), std::string::npos);
}
