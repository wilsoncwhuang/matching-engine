#ifndef TRADE_HPP
#define TRADE_HPP

#include "orderbook/types.hpp"
#include "orderbook/util/timestamp.hpp"

namespace orderbook::core {

using orderbook::util::Timestamp;

struct Trade {
    TradeId  tradeId{INVALID_TRADE_ID};
    Symbol   symbol{};
    OrderId  buyOrderId{INVALID_ORDER_ID};
    OrderId  sellOrderId{INVALID_ORDER_ID};
    Price    price{0.0};
    Quantity quantity{0};
    Timestamp timestamp{};

    Trade() = default;

    Trade(TradeId  id,
          Symbol   symbol,
          OrderId  buyOrderId,
          OrderId  sellOrderId,
          Price    price,
          Quantity quantity,
          Timestamp ts);
};

}

#endif