#ifndef SIMULATED_CLOCK_HPP
#define SIMULATED_CLOCK_HPP

#include "orderbook/util/i_clock.hpp"

namespace orderbook::util {

class SimulatedClock : public IClock {
public:
    SimulatedClock();
    explicit SimulatedClock(const Timestamp& start);

~SimulatedClock() override = default;

Timestamp now() const override;

void set_time(const Timestamp& t);

void advance_time(const Timestamp::duration& delta);

private:
    Timestamp current_;
}; 

}

#endif