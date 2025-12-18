#include "orderbook/core/order_book.hpp"
#include <cassert>

namespace orderbook::core {

OrderBook::OrderBook()
    : bids_(Side::Buy),
      asks_(Side::Sell) 
{
}

std::vector<Trade> OrderBook::submit_order(Order& order) 
{
    assert(order.remaining > 0 && "[order book] submit_order called with non-positive remaining quantity");
    assert(order.price >= 0.0 && "[order book] submit_order called with negative price");

    std::vector<Trade> trades;

    OrderBookSide& oppositeBookSide = opposite_side_of(order.side);
    OrderBookSide& bookSide = side_of(order.side);

    // if FOK (Fill-Or-Kill): check available liquidity first
    if (order.tif == TimeInForce::FOK) {
        Quantity avail = oppositeBookSide.available_quantity_for_order(order);
        if (avail < order.remaining) {
            // cannot fully fill immediately -> kill the order (no trades)
            return trades;
        }
    }

    // match the incoming order against the opposite side
    oppositeBookSide.match(order, trades);

    // if there is remaining quantity, add to the book only for GTC
    if (order.remaining > 0) {
        if (order.tif == TimeInForce::GTC) {
            bookSide.add_order(&order);
        } 
        else {
            // IOC: do not add remaining to book; FOK shouldn't reach here when not fully filled
        }
    }

    return trades;
}

bool OrderBook::cancel_order(Order& order) 
{
    OrderBookSide& bookSide = side_of(order.side);
    bool removed = bookSide.remove_order(order);
    return removed;
}

bool OrderBook::modify_order(Order& order, const ModifyOrderRequest& req) 
{
    if (req.hasNewQuantity && req.newQuantity < order.filled) {
        assert(false && "[order book] modify order new quantity less than already filled");
        return false; // cannot set quantity less than already filled
    }

    OrderBookSide& bookSide = side_of(order.side);
    bool removed = bookSide.remove_order(order);

    if (!removed) {
        assert(false && "[order book] modify_order failed to remove order from book");
        return false; 
    }

    order.price = req.hasNewPrice ? req.newPrice : order.price;
    order.qty = req.hasNewQuantity ? req.newQuantity : order.qty;
    order.remaining = order.qty - order.filled;

    if (order.remaining > 0) {
        bookSide.add_order(&order);
    }

    return true;
}

OrderBookSide& OrderBook::side_of(Side side) 
{
    return (side == Side::Buy) ? bids_ : asks_;
}

const OrderBookSide& OrderBook::side_of(Side side) const 
{
    return (side == Side::Buy) ? bids_ : asks_;
}

OrderBookSide& OrderBook::opposite_side_of(Side side) {
    return (side == Side::Buy) ? asks_ : bids_;
}

const OrderBookSide& OrderBook::opposite_side_of(Side side) const {
    return (side == Side::Buy) ? asks_ : bids_;
}

} 
