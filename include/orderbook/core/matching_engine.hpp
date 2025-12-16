#ifndef MATCHING_ENGINE_HPP
#define MATCHING_ENGINE_HPP

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>
#include <shared_mutex>

#include "orderbook/api/new_order_request.hpp"
#include "orderbook/core/order.hpp"
#include "orderbook/core/trade.hpp"
#include "orderbook/core/order_book.hpp"
#include "orderbook/util/i_clock.hpp"
#include "orderbook/util/id_generator.hpp"
#include "orderbook/report/i_trade_repository.hpp"

namespace orderbook::core {

using orderbook::api::NewOrderRequest;
using orderbook::api::ModifyOrderRequest;
using orderbook::util::IClock;
using orderbook::util::IdGenerator;
using orderbook::report::ITradeRepository;

class MatchingEngine {
public:
    using TradeListener = std::function<void(const std::vector<Trade>&)>;

    MatchingEngine(IClock& clock, ITradeRepository& tradeRepo);

    orderbook::RejectReason validate_new_order(const NewOrderRequest& req) const;
    orderbook::RejectReason validate_modify_order(const Order& order, const ModifyOrderRequest& req) const;

    OrderId new_order(const NewOrderRequest& req);
    bool cancel_order(OrderId orderId);
    bool modify_order(OrderId orderId, const ModifyOrderRequest& req);

    void register_trade_listener(TradeListener listener);

    OrderBook& get_or_create_book(const Symbol& symbol);
    Symbol get_symbol_by_order(OrderId orderId) const;

private:
    std::unordered_map<Symbol, OrderBook> books_;  

    std::unordered_map<OrderId, std::unique_ptr<Order>> ordersRegistry_;

    IClock&             clock_;
    ITradeRepository&   tradeRepo_;
    IdGenerator         orderIdGenerator_;
    IdGenerator         tradeIdGenerator_;

    std::vector<TradeListener> tradeListeners_;

    void on_trades(const std::vector<Trade>& trades);
    void clean_registry(const std::vector<Trade>& trades);

    mutable std::shared_mutex booksMutex_;
    mutable std::shared_mutex registryMutex_;
    mutable std::mutex symbolMutexesGuard_; 
    std::unordered_map<Symbol, std::unique_ptr<std::shared_mutex>> symbolMutexes_;
    mutable std::mutex listenersMutex_;
    std::shared_mutex* get_or_create_symbol_mutex(const Symbol& symbol);  
}; 

}

#endif 
