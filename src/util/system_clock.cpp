#include "orderbook/util/system_clock.hpp"

namespace orderbook::util {

    Timestamp SystemClock::now() const
    {
        return Timestamp::now();
    }

} 
