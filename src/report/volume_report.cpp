#include "orderbook/report/volume_report.hpp"

namespace orderbook::report {

VolumeReport VolumeReport::from_trades(const std::vector<Trade>& trades) 
{
    VolumeReport report;

    if (trades.empty()) return report;

    report.stats_.symbol = trades[0].symbol;

    for (const auto& t : trades) {
        report.stats_.totalQuantity += static_cast<long long>(t.quantity);
        report.stats_.totalNotional += t.price * static_cast<double>(t.quantity);
    }

    return report;
}

} 
