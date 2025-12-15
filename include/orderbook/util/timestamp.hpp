#ifndef TIMESTAMP_HPP
#define TIMESTAMP_HPP

#include <chrono>

namespace orderbook::util { 

class Timestamp {
public:
    using clock         = std::chrono::steady_clock;
    using duration      = std::chrono::nanoseconds;
    using time_point    = std::chrono::time_point<clock, duration>;

    Timestamp();
    explicit Timestamp(const time_point& tp);

    static Timestamp now();

    time_point value() const;

    void set_value(const time_point& tp);

    Timestamp& operator+=(const duration& d);
    Timestamp& operator-=(const duration& d);

private:
    time_point tp_;
};

bool operator==(const Timestamp& lhs, const Timestamp& rhs);
bool operator!=(const Timestamp& lhs, const Timestamp& rhs);
bool operator<(const Timestamp& lhs, const Timestamp& rhs);
bool operator<=(const Timestamp& lhs, const Timestamp& rhs);
bool operator>(const Timestamp& lhs, const Timestamp& rhs);
bool operator>=(const Timestamp& lhs, const Timestamp& rhs);

Timestamp::duration operator-(const Timestamp& lhs, const Timestamp& rhs);

}

#endif