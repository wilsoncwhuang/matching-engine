#pragma once

#include <cstdint>
#include <chrono>
#include <string>

namespace orderbook {

using OrderId = std::uint64_t;
using TradeId = std::uint64_t;
using Price = double; 
using Quantity = std::int64_t;
using Symbol = std::string;

enum class Side {
    Buy,
    Sell
};

enum class OrderType {
    Limit,
    Market
};

enum class TimeInForce {
    GTC, // Good Till Cancelled
    IOC, // Immediate Or Cancel
    FOK  // Fill Or Kill
};

enum class StopType {
    StopMarket,
    StopLimit
};

enum class RejectReason {
    None,
    InvalidPrice,
    InvalidQuantity,
    UnsupportedOrderType,
    UnsupportedTimeInForce
};

// invalid identifiers/values
static constexpr OrderId   INVALID_ORDER_ID = 0;
static constexpr TradeId   INVALID_TRADE_ID = 0;

}