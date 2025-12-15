#ifndef SYSTEMCLOCK_HPP
#define SYSTEMCLOCK_HPP

#include "orderbook/util/i_clock.hpp"

namespace orderbook::util {

class SystemClock : public IClock {
public:
    SystemClock() = default;
    ~SystemClock() override = default;

    Timestamp now() const override;
};

} 

#endif
