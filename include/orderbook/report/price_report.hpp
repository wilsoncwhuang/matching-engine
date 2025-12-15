#ifndef PRICE_REPORT_HPP
#define PRICE_REPORT_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <cstddef>
#include <limits>
#include <cmath>

#include "orderbook/core/trade.hpp"

namespace orderbook::report {

struct PriceStats {
    std::string symbol;

    double      minPrice    = std::numeric_limits<double>::infinity();
    double      maxPrice    = -std::numeric_limits<double>::infinity();
    double      avgPrice    = 0.0;
    double      stdDevPct   = 0.0;
    std::size_t tradeCount  = 0;

    bool isValid() const {
        return tradeCount > 0;
    }
};

class PriceStatsReport {
public:
    using Trade = orderbook::core::Trade;

    PriceStatsReport() = default;

    static PriceStatsReport from_trades(const std::vector<Trade>& trades);

    const PriceStats& stats() const noexcept { return stats_; }

private:
    PriceStats stats_;
};

} 

#endif