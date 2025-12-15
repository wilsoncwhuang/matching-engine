#ifndef ORDER_HPP
#define ORDER_HPP

#include <cstdint>

#include "orderbook/types.hpp"
#include "orderbook/util/timestamp.hpp"

namespace orderbook::core {

using orderbook::util::Timestamp;

struct Order {
    OrderId     orderId{INVALID_ORDER_ID};
    Symbol      symbol{};
    Side        side{Side::Buy};
    OrderType   type{OrderType::Limit};
    TimeInForce tif{TimeInForce::GTC};
    Price       price{0.0};
    Quantity    qty{0};        
    Quantity    remaining{0};  
    Quantity    filled{0};     
    Timestamp   timestamp{};   

    Order() = default;

    Order(OrderId     id,
          Symbol      symbol,
          Side        side,
          OrderType   type,
          TimeInForce tif,
          Price       price,
          Quantity    qty,
          Timestamp   ts);

    // check if order is completely filled
    bool is_filled() const { return qty > 0 && remaining == 0; }

    // add a fill quantity, update filled / remaining
    void add_fill(Quantity q);
};

} 

#endif