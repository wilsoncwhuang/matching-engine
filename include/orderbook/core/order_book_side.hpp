#ifndef ORDER_BOOK_SIDE_HPP
#define ORDER_BOOK_SIDE_HPP

#include <map>
#include <vector>

#include "orderbook/core/order.hpp"
#include "orderbook/core/trade.hpp"
#include "orderbook/core/price_level.hpp"

namespace orderbook::core {

class OrderBookSide {
public:
    explicit OrderBookSide(Side side);

    Side side() const noexcept { return side_; }

    void add_order(Order* order);

    bool remove_order(const Order& order);

    void match(Order& incoming, std::vector<Trade>& trades);

    Quantity available_quantity_for_order(const Order& incoming) const;

    PriceLevel* best_level();
    const PriceLevel* best_level() const;

    std::vector<PriceLevel*> top_k_levels(std::size_t k);
    std::vector<const PriceLevel*> top_k_levels(std::size_t k) const;

private:
    using PriceLevels = std::map<double, PriceLevel>;

    Side   side_;
    PriceLevels priceLevels_;

    PriceLevels::iterator       best_level_it();
    PriceLevels::const_iterator best_level_it() const;

    void clean_side(PriceLevels::iterator it);
};

} 

#endif