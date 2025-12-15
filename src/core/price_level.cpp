#include "orderbook/core/price_level.hpp"
#include <cassert>

namespace orderbook::core {

PriceLevel::PriceLevel(double price)
    : price_(price)
    , volume_(0)
{
}

void PriceLevel::add_order(Order* o) 
{
    if (!o) return;
    if (o->remaining <= 0) return;  
    ordersQueue_.push_back(o);   
    volume_ += o->remaining;
}

Order* PriceLevel::top_order() {
    if (ordersQueue_.empty()) return nullptr;
    return ordersQueue_.front();
}

const Order* PriceLevel::top_order() const {
    if (ordersQueue_.empty()) return nullptr;
    return ordersQueue_.front();
}

void PriceLevel::remove_top_order() {
    if (!ordersQueue_.empty()) {
        volume_ -= ordersQueue_.front()->remaining;
        ordersQueue_.pop_front();
    }
}

bool PriceLevel::remove_order(OrderId orderId) {
    for (auto it = ordersQueue_.begin(); it != ordersQueue_.end(); ++it) {
        if ((*it) && (*it)->orderId == orderId) {
            volume_ -= (*it)->remaining;
            ordersQueue_.erase(it);
            return true;
        }
    }
    return false;
}

void PriceLevel::update_volume(Quantity filledQty) {
    assert(filledQty <= volume_ && "[price level] update_volume would make volume negative");
    volume_ -= filledQty;
}

} 
