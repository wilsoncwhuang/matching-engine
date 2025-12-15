#ifndef VOLUME_REPORT_HPP
#define VOLUME_REPORT_HPP

#include <string>
#include <vector>
#include "orderbook/core/trade.hpp"

namespace orderbook::report {

using Trade = orderbook::core::Trade;

struct VolumeStats {
    std::string symbol;
    long long   totalQuantity = 0;   
    double      totalNotional = 0.0;
};

class VolumeReport {
public:

    VolumeReport() = default;

    static VolumeReport from_trades(const std::vector<Trade>& trades);

    const VolumeStats& stats() const noexcept { return stats_; }

private:
    VolumeStats stats_;
};

} 

#endif