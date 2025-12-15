#ifndef PRICE_LEVEL_HPP
#define PRICE_LEVEL_HPP

#include <deque>

#include "orderbook/types.hpp"
#include "orderbook/core/order.hpp"

namespace orderbook::core {

class PriceLevel {
public:
    using OrdersQueue = std::deque<Order*>;

    explicit PriceLevel(Price price = 0.0);

    void add_order(Order* o);

    Order* top_order(); 
    const Order* top_order() const;

    void remove_top_order();

    bool remove_order(OrderId orderId);

    void update_volume(Quantity filledQty);

    Price price() const { return price_; }

    Quantity volume() const { return volume_; }

    OrdersQueue& orders() { return ordersQueue_; }
    const OrdersQueue& orders() const { return ordersQueue_; }

    bool empty() const { return ordersQueue_.empty(); }
    std::size_t size() const { return ordersQueue_.size(); }

private:
    Price       price_;
    Quantity    volume_;
    OrdersQueue ordersQueue_;
};

} 

#endif 