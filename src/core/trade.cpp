#include "orderbook/core/trade.hpp"

namespace orderbook::core {

Trade::Trade(TradeId  id,
             Symbol   symbol_,
             OrderId  buyOrderId_,
             OrderId  sellOrderId_,
             Price    price_,
             Quantity quantity_,
             Timestamp ts)
    : tradeId{id}
    , symbol{std::move(symbol_)}
    , buyOrderId{buyOrderId_}
    , sellOrderId{sellOrderId_}
    , price{price_}
    , quantity{quantity_}
    , timestamp{ts}
{
}

}
