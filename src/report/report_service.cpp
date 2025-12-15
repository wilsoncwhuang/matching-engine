#include "orderbook/report/report_service.hpp"

namespace orderbook::report {

VolumeReport ReportService::volume_between(const Symbol& symbol, Timestamp start, Timestamp end) 
{
    auto trades = repo_.trades_between(symbol, start, end);
    return VolumeReport::from_trades(trades);
}

VolumeReport ReportService::volume_all(const Symbol& symbol) 
{
    auto trades = repo_.trades_all(symbol);
    return VolumeReport::from_trades(trades);
}

PriceStatsReport ReportService::price_between(const Symbol& symbol, Timestamp start, Timestamp end) 
{
    auto trades = repo_.trades_between(symbol, start, end);
    return PriceStatsReport::from_trades(trades);
}

PriceStatsReport ReportService::price_all(const Symbol& symbol) 
{
    auto trades = repo_.trades_all(symbol);
    return PriceStatsReport::from_trades(trades);
}

} 
