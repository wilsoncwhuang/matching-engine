#ifndef ICLOCK_HPP
#define ICLOCK_HPP

#include "orderbook/util/Timestamp.hpp"

namespace orderbook::util {

    class IClock {
    public:
        virtual ~IClock();
        virtual Timestamp now() const = 0;
    };

} 

#endif