#ifndef INTERNAL_TRADE_REPOSITORY_HPP
#define INTERNAL_TRADE_REPOSITORY_HPP

#include <vector>
#include <unordered_map>
#include <mutex>

#include "orderbook/report/i_trade_repository.hpp"

namespace orderbook::report {

class InternalTradeRepository : public ITradeRepository {
public:
    void add_trades(const std::vector<Trade>& trades) override;

    std::vector<Trade> trades_between(const Symbol& symbol, Timestamp start, Timestamp end) override;

    std::vector<Trade> trades_all(const Symbol& symbol) override;

private:
    std::unordered_map<Symbol, std::vector<Trade>> tradesBySymbol_;  
    std::mutex mutex_;
};

}

#endif