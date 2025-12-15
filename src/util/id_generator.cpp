#include "orderbook/util/id_generator.hpp"

namespace orderbook::util {

    IdGenerator::IdGenerator()
        : counter_{1}
    {
    }

    IdGenerator::IdGenerator(value_type start)
        : counter_{start}
    {
    }

    IdGenerator::value_type IdGenerator::next()
    {
        return counter_.fetch_add(1, std::memory_order_relaxed);
    }

    IdGenerator::value_type IdGenerator::current() const
    {
        return counter_.load(std::memory_order_relaxed);
    }

} 
