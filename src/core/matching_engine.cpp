#include "orderbook/core/matching_engine.hpp"
#include <unordered_set>
#include <cassert>

namespace orderbook::core {

MatchingEngine::MatchingEngine(IClock& clock,
                               ITradeRepository& tradeRepo)
    : books_()
    , clock_(clock)
    , tradeRepo_(tradeRepo)
    , orderIdGenerator_()
    , tradeIdGenerator_()
{
}

orderbook::RejectReason MatchingEngine::validate_new_order(const NewOrderRequest& req) const
{
    if (req.quantity <= 0) return orderbook::RejectReason::InvalidQuantity;
    if (req.type == orderbook::OrderType::Limit) {
        if (req.price <= 0.0) return orderbook::RejectReason::InvalidPrice;
    }
    if (req.type != orderbook::OrderType::Limit && req.type != orderbook::OrderType::Market) {
        return orderbook::RejectReason::UnsupportedOrderType;
    }
    return orderbook::RejectReason::None;
}

orderbook::RejectReason MatchingEngine::validate_modify_order(const Order& order, const ModifyOrderRequest& req) const
{
    if (order.tif != TimeInForce::GTC) return orderbook::RejectReason::UnsupportedTimeInForce;
    if (req.hasNewQuantity && req.newQuantity < order.filled) return orderbook::RejectReason::InvalidQuantity;
    if (req.hasNewPrice && order.type == orderbook::OrderType::Market) return orderbook::RejectReason::UnsupportedOrderType;
    if (req.hasNewPrice && req.newPrice <= 0.0) return orderbook::RejectReason::InvalidPrice;
    return orderbook::RejectReason::None;
}

OrderId MatchingEngine::new_order(const NewOrderRequest& req) 
{
    auto vr = validate_new_order(req);
    if (vr != orderbook::RejectReason::None) {
        return INVALID_ORDER_ID; 
    }

    std::unique_lock<std::shared_mutex> symwLock(get_or_create_symbol_mutex(req.symbol));

    // create order
    auto ptr = std::make_unique<Order>();
    Order& o = *ptr;
    o.orderId   = orderIdGenerator_.next();   
    o.symbol    = req.symbol;
    o.side      = req.side;
    o.type      = req.type;
    o.tif       = req.tif;
    o.price     = req.price;
    o.qty       = req.quantity;
    o.remaining = req.quantity;
    o.filled    = 0;
    o.timestamp = clock_.now();

    OrderId id = o.orderId;

    if (o.type == OrderType::Market && o.tif == TimeInForce::GTC) {
        o.tif = TimeInForce::IOC;
    }

    {
        std::unique_lock<std::shared_mutex> regwLock(registryMutex_);
        ordersRegistry_.emplace(id, std::move(ptr));
    }

    // get or create order book for this symbol and submit order
    OrderBook& book = get_or_create_book(o.symbol);
    std::vector<Trade> trades = book.submit_order(o);
    
    // if order is not GTC and still has remaining, erase from registry
    if (o.tif != TimeInForce::GTC && o.remaining > 0) {
        std::unique_lock<std::shared_mutex> regwLock(registryMutex_);
        ordersRegistry_.erase(id);
    }

    // process trades: assign tradeId, timestamp, store into tradeRepo, notify listeners
    if (!trades.empty()) {
        std::unique_lock<std::shared_mutex> regwLock(registryMutex_);
        clean_registry(trades);
    }

    if (!trades.empty()) {
        auto ts = clock_.now();
        for (auto& t : trades) {
            t.tradeId   = tradeIdGenerator_.next();
            t.timestamp = ts;
        }
    }

    symwLock.unlock();

    if (!trades.empty()) {
        tradeRepo_.add_trades(trades);
        on_trades(trades);
    }

    return id;
}

bool MatchingEngine::cancel_order(OrderId orderId) 
{
    // map to symbol
    Symbol sym;
    {
        std::shared_lock<std::shared_mutex> regrLock(registryMutex_);
        auto it = ordersRegistry_.find(orderId);
        if (it == ordersRegistry_.end()) {
            return false;
        }
        sym = it->second->symbol;
    }

    std::unique_lock<std::shared_mutex> symwLock(get_or_create_symbol_mutex(sym));

    // find order in registry
    Order* optr = nullptr;
    {
        std::unique_lock<std::shared_mutex> regrLock(registryMutex_);
        auto it = ordersRegistry_.find(orderId);
        if (it == ordersRegistry_.end()) {
            return false;
        }
        optr = it->second.get();
    }

    // get order book for this symbol and cancel
    OrderBook* bptr = nullptr;
    {
        std::shared_lock<std::shared_mutex> bookrLock(booksMutex_);
        auto bit = books_.find(sym);
        if (bit == books_.end()) {
            assert(false && "[matching engine] registry and books out of sync on cancel_order");
            std::unique_lock<std::shared_mutex> regwLock(registryMutex_);
            ordersRegistry_.erase(orderId); // gracefully handle
            return false;
        }
        bptr = &bit->second;
    }

    bool removed = bptr->cancel_order(*optr);

    if (removed) {
        std::unique_lock<std::shared_mutex> regwLock(registryMutex_);
        ordersRegistry_.erase(orderId);
        return true;
    } 

    {
        assert(false && "[matching engine] registry and books out of sync on cancel_order");
        std::unique_lock<std::shared_mutex> wlock(registryMutex_);
        auto it = ordersRegistry_.find(orderId);
        if (it != ordersRegistry_.end() && it->second->remaining == 0) {
            ordersRegistry_.erase(it);
        }
    }
    return false;
}

bool MatchingEngine::modify_order(OrderId orderId, const ModifyOrderRequest& req) 
{
    // map to symbol
    Symbol sym;
    {
        std::shared_lock<std::shared_mutex> regrLock(registryMutex_);
        auto it = ordersRegistry_.find(orderId);
        if (it == ordersRegistry_.end()) {
            return false;
        }
        sym = it->second->symbol;
    }

    std::unique_lock<std::shared_mutex> symwLock(get_or_create_symbol_mutex(sym));

    // find order in registry
    Order* optr = nullptr;
    {
        std::unique_lock<std::shared_mutex> regrLock(registryMutex_);
        auto it = ordersRegistry_.find(orderId);
        if (it == ordersRegistry_.end()) {
            return false;
        }
        optr = it->second.get();
    }

    // validate modification against current order state
    auto vr = validate_modify_order(*optr, req);
    if (vr != orderbook::RejectReason::None) {
        return false;
    }

    OrderBook& book = get_or_create_book(sym);

    // decide whether rematching is needed: price change crossing opposite best price, or market order
    bool priceChanged = req.hasNewPrice;
    Price newPrice = priceChanged ? req.newPrice : optr->price;
    bool willRematch = false;

    if (optr->type == OrderType::Market) {
        willRematch = true;
    }

    if (!willRematch && priceChanged) {
        const OrderBookSide& opposite = (optr->side == Side::Buy) ? book.asks() : book.bids();
        const PriceLevel* best = opposite.best_level();
        if (best) {
            double bestPrice = best->price();
            if (optr->side == Side::Buy) {
                if (newPrice >= bestPrice) willRematch = true;
            } else {
                if (newPrice <= bestPrice) willRematch = true;
            }
        }
    }

    if (!willRematch) {
        return book.modify_order(*optr, req);
    }

    // prepare a temporary Order copy to run FOK pre-check without mutating registry
    Order temp = *optr;
    temp.price = req.hasNewPrice ? req.newPrice : temp.price;
    temp.qty = req.hasNewQuantity ? req.newQuantity : temp.qty;
    temp.remaining = temp.qty - temp.filled;

    if (temp.tif == TimeInForce::FOK) {
        const OrderBookSide& opposite = (temp.side == Side::Buy) ? book.asks() : book.bids();
        const Quantity avail = opposite.available_quantity_for_order(temp);
        if (avail < temp.remaining) {
            return false; 
        }
    }

    // get the order book for this symbol and cancel the old order
    bool removed = book.cancel_order(*optr);
    if (!removed) {
        assert(false && "[matching engine] registry and books out of sync on modify_order");
        std::unique_lock<std::shared_mutex> wlock(registryMutex_);
        auto it = ordersRegistry_.find(orderId);
        if (it != ordersRegistry_.end() && it->second->remaining == 0) {
            ordersRegistry_.erase(it);
        }
        return false;
    }
    
    optr->price = temp.price;
    optr->qty = temp.qty;
    optr->remaining = temp.remaining;

    std::vector<Trade> trades = book.submit_order(*optr);

    if (!trades.empty()) {
        std::unique_lock<std::shared_mutex> wlock(registryMutex_);
        clean_registry(trades); 
    }

    if (!trades.empty()) {
        const auto ts = clock_.now();
        for (auto& t : trades) {
            t.tradeId   = tradeIdGenerator_.next();
            t.timestamp = ts;
        }
    }

    symwLock.unlock();

    if (!trades.empty()) {
        tradeRepo_.add_trades(trades);
        on_trades(trades);
    }

    return true;
}

void MatchingEngine::register_trade_listener(TradeListener listener) 
{
    std::lock_guard<std::mutex> lock(listenersMutex_);
    tradeListeners_.push_back(std::move(listener));
}

OrderBook& MatchingEngine::get_or_create_book(const Symbol& symbol) 
{
    {
        std::shared_lock<std::shared_mutex> rlock(booksMutex_);
        auto it = books_.find(symbol);
        if (it != books_.end()) {
            return it->second;
        }
    }

    std::unique_lock<std::shared_mutex> wlock(booksMutex_);
    auto [it, inserted] = books_.try_emplace(symbol, OrderBook{});
    return it->second;
}

void MatchingEngine::on_trades(const std::vector<Trade>& trades)
{
    std::vector<TradeListener> copyTradeListeners;
    {
        std::lock_guard<std::mutex> lk(listenersMutex_);
        if (tradeListeners_.empty()) return;
        copyTradeListeners = tradeListeners_;
    } 

    for (auto& listener : copyTradeListeners) listener(trades);
}

void MatchingEngine::clean_registry(const std::vector<Trade>& trades) 
{
    std::unordered_set<OrderId> ids;
    for (const auto& t : trades) {
        if (t.buyOrderId != orderbook::INVALID_ORDER_ID) ids.insert(t.buyOrderId);
        if (t.sellOrderId != orderbook::INVALID_ORDER_ID) ids.insert(t.sellOrderId);
    }
    for (auto id : ids) {
        auto rit = ordersRegistry_.find(id);
        if (rit != ordersRegistry_.end()) {
            if (rit->second->remaining == 0) {
                ordersRegistry_.erase(rit);
            }
        }
    }
}

Symbol MatchingEngine::get_symbol_by_order(OrderId orderId) const {
    std::shared_lock<std::shared_mutex> regrLock(registryMutex_);
    auto it = ordersRegistry_.find(orderId);
    if (it != ordersRegistry_.end()) {
        return it->second->symbol;
    }
    return "";
}

std::shared_mutex& MatchingEngine::get_or_create_symbol_mutex(const Symbol& symbol) {
    {  
        std::shared_lock<std::shared_mutex> rlock(booksMutex_);
        auto it = symbolMutexes_.find(symbol);
        if (it != symbolMutexes_.end()) return *it->second;
    }
    {   
        std::unique_lock<std::shared_mutex> wlock(booksMutex_);
        auto& ptr = symbolMutexes_[symbol];
        if (!ptr) ptr = std::make_unique<std::shared_mutex>();
        return *ptr;
    }
}

} 