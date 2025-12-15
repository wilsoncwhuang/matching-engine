#include "orderbook/util/timestamp.hpp"

namespace orderbook::util {

    Timestamp::Timestamp() : tp_(clock::now()) {}

    Timestamp::Timestamp(const time_point& tp) : tp_(tp) {}

    Timestamp Timestamp::now() {
        return Timestamp{
            std::chrono::time_point_cast<duration>(clock::now())
        };
    }

    Timestamp::time_point Timestamp::value() const {
        return tp_;
    }

    void Timestamp::set_value(const time_point& tp) {
        tp_ = tp;
    }

    Timestamp& Timestamp::operator+=(const duration& d) {
        tp_ += d;
        return *this;
    }

    Timestamp& Timestamp::operator-=(const duration& d) {
        tp_ -= d;
        return *this;
    }

    bool operator==(const Timestamp& lhs, const Timestamp& rhs) {
        return lhs.value() == rhs.value();
    }

    bool operator!=(const Timestamp& lhs, const Timestamp& rhs) {
        return !(lhs == rhs);
    }

    bool operator<(const Timestamp& lhs, const Timestamp& rhs) {
        return lhs.value() < rhs.value();
    }

    bool operator<=(const Timestamp& lhs, const Timestamp& rhs) {
        return (lhs < rhs) || (lhs == rhs);
    }

    bool operator>(const Timestamp& lhs, const Timestamp& rhs) {
        return !(lhs <= rhs);
    }

    bool operator>=(const Timestamp& lhs, const Timestamp& rhs) {
        return !(lhs < rhs);
    }

    Timestamp::duration operator-(const Timestamp& lhs, const Timestamp& rhs) {
        return lhs.value() - rhs.value();
    }

}