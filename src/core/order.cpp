#include <cassert>

#include "orderbook/core/order.hpp"

namespace orderbook::core {

Order::Order(OrderId     id,
             Symbol      symbol_,
             Side        side_,
             OrderType   type_,
             TimeInForce tif_,
             Price       price_,
             Quantity    qty_,
             Timestamp   ts)
    : orderId{id}
    , symbol{std::move(symbol_)}
    , side{side_}
    , type{type_}
    , tif{tif_}
    , price{price_}
    , qty{qty_}
    , remaining{qty_}   
    , filled{0}
    , timestamp{ts}
{
}

void Order::add_fill(Quantity q)
{
    if (q <= 0) {
        return;
    }

    if (q > remaining) {
        q = remaining;
    }

    filled    += q;
    remaining -= q;
}

} 
