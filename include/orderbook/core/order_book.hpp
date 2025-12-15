#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include <unordered_map>
#include <vector>

#include "orderbook/core/order.hpp"
#include "orderbook/core/trade.hpp"
#include "orderbook/core/order_book_side.hpp"
#include "orderbook/api/modify_order_request.hpp"

namespace orderbook::core {

using orderbook::api::ModifyOrderRequest; 

class OrderBook {
public:
    OrderBook();

    std::vector<Trade> submit_order(Order& order);

    bool cancel_order(Order& order);

    bool modify_order(Order& order, const ModifyOrderRequest& req);

    const OrderBookSide& bids() const noexcept { return bids_; }
    const OrderBookSide& asks() const noexcept { return asks_; }

private:
    OrderBookSide bids_;
    OrderBookSide asks_;

    OrderBookSide& side_of(Side side);
    const OrderBookSide& side_of(Side side) const;
    OrderBookSide& opposite_side_of(Side side);
    const OrderBookSide& opposite_side_of(Side side) const;
};

} 

#endif