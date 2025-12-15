# Matching Engine — Thread-Safe, Multi-Symbol Order Matching System

![C++](https://img.shields.io/badge/C%2B%2B-20-blue)
![Windows](https://img.shields.io/badge/Windows-10%2B-success)
![Thread-Safe](https://img.shields.io/badge/Thread--Safe-Yes-success)

A high-performance, thread-safe order matching engine implemented in C++20. Supports multiple TimeInForce types, order modification/cancellation, and real-time trade reporting.

## Supported Features

### Order Management
- **New Order** - Support for Limit and Market orders
- **Cancel Order** - Quick removal of unfilled orders
- **Modify Order** - Support for modifying quantity and price (GTC orders only)
- **Multi-Symbol Support** - Independent order books for each symbol

### TimeInForce Types
- **GTC** (Good Till Cancelled) - Remains until cancelled or fully filled
- **IOC** (Immediate Or Cancel) - Execute immediately or cancel unfilled portion
- **FOK** (Fill Or Kill) - Fully fill or cancel entire order
- **Market Order** - Automatically converted to IOC, fills at best available price

### Performance Features
- **Thread-Safe** - Uses `std::shared_mutex` for multi-threaded support per symbol
- **Scalable Architecture** - Supports custom clock implementations and trade repositories

### Reporting System
- **Volume Report** - Aggregated trade volume by symbol
- **Price Report** - Trade analysis at price level granularity

## Getting Started

### Requirements
- CMake 3.20+
- MSYS2 MinGW64 with C++20 support

### Build and Run

```bash
mkdir build
cd build
cmake ..
cmake --build .

# Web UI Demo (loads test cases from cases.txt)
./web_demo.exe
# Visit http://localhost:8080 in browser

# Or run Multithread stress test
./multithread_test.exe
```

## Usage Guide

### Basic Usage

```cpp
#include "orderbook/core/matching_engine.hpp"
#include "orderbook/util/system_clock.hpp"
#include "orderbook/report/internal_trade_repository.hpp"

using namespace orderbook;

// Initialize
SystemClock clock;
InternalTradeRepository repo;
MatchingEngine engine(clock, repo);

// Create order
NewOrderRequest req{
    .symbol = "AAPL",
    .side = Side::Buy,
    .type = OrderType::Limit,
    .tif = TimeInForce::GTC,
    .price = 150.50,
    .quantity = 100
};

OrderId id = engine.new_order(req);

// Modify order (GTC only)
ModifyOrderRequest modify_req{
    .hasNewQuantity = true,
    .newQuantity = 150,
    .hasNewPrice = true,
    .newPrice = 151.00
};
engine.modify_order(id, modify_req);

// Cancel order
engine.cancel_order(id);

// Listen for trades
engine.register_trade_listener([](const std::vector<Trade>& trades) {
    for (const auto& t : trades) {
        std::cout << "Trade: " << t.symbol << " @ " << t.price << " x " << t.quantity << std::endl;
    }
});
```

## Test Cases

[cases.txt](cases.txt) contains 12 demonstration scenarios showcasing all features:

1. **Basic Order Creation** - GTC orders
2. **Partial Matching** - Multiple orders filling simultaneously
3. **Order Modification** - Modify quantity and price
4. **Rematching After Modification** - Price crossing best bid/ask
5. **Order Cancellation** - Remove pending orders
6. **IOC Orders** - Immediate fill or cancel
7. **FOK Orders** - Complete fill or cancel entire order
8. **Market Orders** - Auto-converted to IOC
9. **Multi-Symbol Trading** - Independent order books
10. **Deep Order Book** - Multiple price levels
11. **Modify Then Cancel** - Sequential operations
12. **Trade Reporting** - Report generation

### cases.txt File Format

Each line defines an operation command:

```
# Create new order
new <symbol> <side> <type> <tif> <price> <quantity>
new AAPL BUY LIMIT GTC 100 10

# Modify order
modify <orderId> <newQuantity> <newPrice>
modify 1 20 101

# Cancel order
cancel <orderId>
cancel 2

# End demo
quit
```

**Parameter Description:**
- `symbol`: Stock symbol (e.g., AAPL, TSLA)
- `side`: BUY or SELL
- `type`: LIMIT or MARKET
- `tif`: GTC, IOC, or FOK
- `price`: Order price
- `quantity`: Order quantity
- `orderId`: Order ID (auto-assigned by system, starting from 1)

### Running Tests in Web UI

Click the "Auto Play" button in the Web UI to automatically execute all test cases from cases.txt.
