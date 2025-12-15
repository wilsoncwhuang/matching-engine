#ifndef NEW_ORDER_REQUEST_HPP
#define NEW_ORDER_REQUEST_HPP

#include <cstdint>
#include <string>

#include "orderbook/types.hpp"

namespace orderbook::api {

struct NewOrderRequest {
    
    Symbol      symbol;
    Side        side;
    OrderType   type;
    TimeInForce tif;
    Price       price;
    Quantity    quantity;

    NewOrderRequest() = default;

    NewOrderRequest(Symbol      symbol,
                    Side        side,
                    OrderType   type,
                    TimeInForce tif,
                    Price       price,
                    Quantity    quantity)
    : symbol{std::move(symbol)}
    , side{side}
    , type{type}
    , tif{tif}
    , price{price}
    , quantity{quantity}
    {
    }
                    
};

} 

#endif
