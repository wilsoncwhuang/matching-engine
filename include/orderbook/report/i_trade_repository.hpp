#ifndef I_TRADE_REPOSITORY_HPP
#define I_TRADE_REPOSITORY_HPP

#include <vector>
#include "orderbook/core/trade.hpp"

namespace orderbook::report {

using orderbook::core::Trade;
using orderbook::util::Timestamp;

class ITradeRepository {
public:
    virtual ~ITradeRepository() = default;

    virtual void add_trades(const std::vector<Trade>& trades) = 0;

    virtual std::vector<Trade> trades_between(const Symbol& symbol, Timestamp start, Timestamp end) = 0;

    virtual std::vector<Trade> trades_all(const Symbol& symbol) = 0;
};

} 

#endif
