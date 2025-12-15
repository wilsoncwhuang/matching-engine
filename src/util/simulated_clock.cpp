#include "orderbook/util/simulated_clock.hpp"

namespace orderbook::util {

    SimulatedClock::SimulatedClock()
        : current_{}
    {
    }

    SimulatedClock::SimulatedClock(const Timestamp& start)
        : current_{start}
    {
    }

    Timestamp SimulatedClock::now() const
    {
        return current_;
    }

    void SimulatedClock::set_time(const Timestamp& t)
    {
        current_ = t;
    }

    void SimulatedClock::advance_time(const Timestamp::duration& delta)
    {
        current_ += delta;
    }

} 
