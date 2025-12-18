#include "orderbook/core/order_book_side.hpp"

#include <algorithm>
#include <cassert>

namespace orderbook::core {

OrderBookSide::OrderBookSide(Side side)
    : side_(side) 
{
}

void OrderBookSide::add_order(Order* order) 
{
    if (!order) return;
    if (order->remaining <= 0) return;

    double price = order->price;
    auto it = priceLevels_.find(price);
    if (it == priceLevels_.end()) {
        it = priceLevels_.emplace(price, PriceLevel(price)).first;
    }
    it->second.add_order(order);
}

bool OrderBookSide::remove_order(const Order& order) 
{
    auto it = priceLevels_.find(order.price);
    if (it == priceLevels_.end()) {
        assert(false && "[order book side] remove_order price level not found");
        return false;
    }
    
    PriceLevel& level = it->second;
    bool removed = level.remove_order(order.orderId);
    
    if (!removed) {
        assert(false && "[order book side] remove_order order not found in price level");
        return false;
    }
    
    // Clean up empty price level
    if (level.empty()) {
        priceLevels_.erase(it);
    }
    
    return true;
}

void OrderBookSide::match(Order& incoming, std::vector<Trade>& trades) 
{
    assert(incoming.remaining > 0 && "[order book side] match called with non-positive remaining quantity");
    assert((incoming.type == OrderType::Market || incoming.price > 0.0) && "[order book side] limit order match called with negative price");

    while (incoming.remaining > 0) {
        auto it = best_level_it();
        if (it == priceLevels_.end()) break;

        double bestPrice = it->second.price();
        // check limit order price crossing 
        if (incoming.type == OrderType::Limit) {
            if (incoming.side == Side::Buy) {
                if (bestPrice > incoming.price) {
                    break;
                }
            } 
            else { 
                if (bestPrice < incoming.price) {
                    break;
                }
            }
        }
        
        PriceLevel& level = it->second;
        Order* resting = level.top_order();
        assert(resting && "[order book side] best_level_it() should guarantee non-empty level");

        // trade price is resting order price
        Quantity matchQty = std::min(incoming.remaining, resting->remaining);
        double tradePrice = resting->price; 

        // update order's filled / remaining
        incoming.add_fill(matchQty);
        resting->add_fill(matchQty);

        // record trade
        Trade trade;
        trade.symbol = incoming.symbol;
        trade.price = tradePrice;
        trade.quantity = matchQty;
        trade.timestamp = incoming.timestamp; // temp, will be updated later
        if (incoming.side == Side::Buy) {
            trade.buyOrderId = incoming.orderId;
            trade.sellOrderId = resting->orderId;
        } 
        else {
            trade.buyOrderId = resting->orderId;
            trade.sellOrderId = incoming.orderId;
        }
        trades.push_back(trade);

        if (resting->remaining == 0) {
            level.remove_top_order();
            clean_side(it);
        } 
        else {
            level.update_volume(matchQty);
        }
    }
}

Quantity OrderBookSide::available_quantity_for_order(const Order& incoming) const 
{
    Quantity total = 0;

    if (priceLevels_.empty()) return 0;

    if (incoming.type == OrderType::Limit) {
        if (side_ == Side::Sell) {
            for (auto it = priceLevels_.begin(); it != priceLevels_.end(); ++it) {
                double p = it->first;
                if (p > incoming.price) break;
                total += it->second.volume(); 
                if (total >= incoming.remaining) return total;
            }
        } 
        else { 
            for (auto it = priceLevels_.rbegin(); it != priceLevels_.rend(); ++it) {
                double p = it->first;
                if (p < incoming.price) break;
                total += it->second.volume();  
                if (total >= incoming.remaining) return total;
            }
        }
    } 
    else {
        for (auto& kv : priceLevels_) {
            total += kv.second.volume();  
            if (total >= incoming.remaining) return total;
        }
    }

    return total;
}

PriceLevel* OrderBookSide::best_level() 
{
    auto it = best_level_it();
    if (it == priceLevels_.end()) return nullptr;
    return &it->second;
}

const PriceLevel* OrderBookSide::best_level() const 
{
    auto it = best_level_it();
    if (it == priceLevels_.end()) return nullptr;
    return &it->second;
}

std::vector<PriceLevel*> OrderBookSide::top_k_levels(std::size_t k) 
{
    std::vector<PriceLevel*> levels;
    if (k == 0 || priceLevels_.empty()) return levels;

    if (side_ == Side::Buy) {
        for (auto it = priceLevels_.rbegin(); it != priceLevels_.rend() && levels.size() < k; ++it) {
            if (!it->second.empty()) {
                levels.push_back(&it->second);
            }
        }
    } 
    else {
        for (auto it = priceLevels_.begin(); it != priceLevels_.end() && levels.size() < k; ++it) {
            if (!it->second.empty()) {
                levels.push_back(&it->second);
            }
        }
    }

    return levels;
}

std::vector<const PriceLevel*> OrderBookSide::top_k_levels(std::size_t k) const
{
    std::vector<const PriceLevel*> levels;
    if (k == 0 || priceLevels_.empty()) return levels;

    if (side_ == Side::Buy) {
        for (auto it = priceLevels_.rbegin(); it != priceLevels_.rend() && levels.size() < k; ++it) {
            if (!it->second.empty()) {
                levels.push_back(&it->second);
            }
        }
    } 
    else {
        for (auto it = priceLevels_.begin(); it != priceLevels_.end() && levels.size() < k; ++it) {
            if (!it->second.empty()) {
                levels.push_back(&it->second);
            }
        }
    }

    return levels;
}

OrderBookSide::PriceLevels::iterator OrderBookSide::best_level_it() 
{
    if (priceLevels_.empty()) return priceLevels_.end();

    if (side_ == Side::Buy) {
        auto it = priceLevels_.end();
        do {
            --it;
            if (!it->second.empty()) return it;
        } while (it != priceLevels_.begin());
        return priceLevels_.end();
    } 
    else {
        for (auto it = priceLevels_.begin(); it != priceLevels_.end(); ++it) {
            if (!it->second.empty()) return it;
        }
        return priceLevels_.end();
    }
}

OrderBookSide::PriceLevels::const_iterator OrderBookSide::best_level_it() const 
{
    if (priceLevels_.empty()) return priceLevels_.end();

    if (side_ == Side::Buy) {
        auto it = priceLevels_.end();
        do {
            --it;
            if (!it->second.empty()) return it;
        } while (it != priceLevels_.begin());
        return priceLevels_.end();
    } 
    else {
        for (auto it = priceLevels_.begin(); it != priceLevels_.end(); ++it) {
            if (!it->second.empty()) return it;
        }
        return priceLevels_.end();
    }
}

void OrderBookSide::clean_side(PriceLevels::iterator it) {
    if (it != priceLevels_.end() && it->second.empty()) {
        priceLevels_.erase(it);
    }
}

} 
