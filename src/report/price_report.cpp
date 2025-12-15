#include "orderbook/report/price_report.hpp"

namespace orderbook::report {

PriceStatsReport PriceStatsReport::from_trades(const std::vector<Trade>& trades) 
{
    PriceStatsReport report;

    if (trades.empty()) return report;

    report.stats_.symbol = trades[0].symbol;

    double sumPrice = 0.0;
    double sumSquares = 0.0;

    for (const auto& t : trades) {
        if (t.price < report.stats_.minPrice) {
            report.stats_.minPrice = t.price;
        }
        if (t.price > report.stats_.maxPrice) {
            report.stats_.maxPrice = t.price;
        }

        sumPrice += t.price;
        sumSquares += t.price * t.price;
        report.stats_.tradeCount += 1;
    }

    report.stats_.avgPrice = sumPrice / static_cast<double>(report.stats_.tradeCount);

    double variance = (sumSquares / static_cast<double>(report.stats_.tradeCount)) - 
                      (report.stats_.avgPrice * report.stats_.avgPrice);
    if (variance < 0.0) variance = 0.0;  
    double stdDev = std::sqrt(variance);
    report.stats_.stdDevPct = (report.stats_.avgPrice > 0.0) ? (stdDev / report.stats_.avgPrice) * 100.0 : 0.0;
    return report;
}

} 
