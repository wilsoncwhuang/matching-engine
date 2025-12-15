#include "orderbook/report/internal_trade_repository.hpp"

namespace orderbook::report {

void InternalTradeRepository::add_trades(const std::vector<Trade>& trades) 
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& trade : trades) {
        tradesBySymbol_[trade.symbol].push_back(trade);
    }
}

std::vector<Trade> InternalTradeRepository::trades_between(const Symbol& symbol, Timestamp start, Timestamp end) 
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Trade> result;
    
    auto it = tradesBySymbol_.find(symbol);
    if (it == tradesBySymbol_.end()) return result;
    
    for (const auto& t : it->second) {
        if (t.timestamp >= start && t.timestamp <= end) {
            result.push_back(t);
        }
    }
    return result;
}

std::vector<Trade> InternalTradeRepository::trades_all(const Symbol& symbol) 
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = tradesBySymbol_.find(symbol);
    if (it == tradesBySymbol_.end()) return std::vector<Trade>();
    
    return it->second;
}

} 
