#pragma once

#include <chrono>
#include <cstdint>

namespace tiny::wm {

/**
 * @short The turns of `_NET_WM_SYNC_REQUEST`: the window manager asks the
 * client to draw a new size and does not resize it again before the client
 * says it is done.
 *
 * The client does it by setting its counter to the number of the request.
 * Without the wait the frame follows the pointer faster than the client draws,
 * and what is between them flickers. A client that does not answer (it hangs,
 * it forgot the counter) is not waited for longer than `timeout`.
 */
class SyncGate {
  public:
    using Clock = std::chrono::steady_clock;

    static constexpr std::chrono::milliseconds timeout{500};

    //! May the next size be sent? `counter` is the value of the counter of the
    //! client now
    bool is_open(uint64_t counter, Clock::time_point now) const
    {
        return !waiting || counter >= sent || now - asked_at > timeout;
    }

    //! The number of a new request; the gate closes until the client answers
    uint64_t request(Clock::time_point now)
    {
        waiting = true;
        asked_at = now;
        return ++sent;
    }

    //! Forget the requests (a new resize starts)
    void reset() { waiting = false; }

    bool is_waiting() const { return waiting; }

    uint64_t get_sent() const { return sent; }

  private:
    uint64_t sent = 0;
    bool waiting = false;
    Clock::time_point asked_at;
};

} // namespace tiny::wm
