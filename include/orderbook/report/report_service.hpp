#ifndef REPORT_SERVICE_HPP
#define REPORT_SERVICE_HPP

#include "orderbook/report/i_trade_repository.hpp"
#include "orderbook/report/volume_report.hpp"
#include "orderbook/report/price_report.hpp"
#include "orderbook/core/trade.hpp"
#include "orderbook/util/timestamp.hpp" 

namespace orderbook::report {

class ReportService {
public:

    explicit ReportService(ITradeRepository& repo)
        : repo_(repo) 
    {
    }

    VolumeReport volume_between(const Symbol& symbol, Timestamp start, Timestamp end);
    VolumeReport volume_all(const Symbol& symbol);

    PriceStatsReport price_between(const Symbol& symbol, Timestamp start, Timestamp end);
    PriceStatsReport price_all(const Symbol& symbol);

private:
    ITradeRepository& repo_;
};

} 

#endif 