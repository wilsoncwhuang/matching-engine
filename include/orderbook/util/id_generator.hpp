#ifndef ID_GENERATOR_HPP
#define ID_GENERATOR_HPP

#include <atomic>
#include <cstdint>

namespace orderbook::util {

class IdGenerator {
public:
    using value_type = std::uint64_t;

    IdGenerator();
    explicit IdGenerator(value_type start);

    value_type next();

    value_type current() const;

private:
    std::atomic<value_type> counter_;  // thread-safe counter for orderId / tradeId
};

} 

#endif