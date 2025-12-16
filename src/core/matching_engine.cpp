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
    if (vr != orderbook::RejectReason::None) return INVALID_ORDER_ID;

    // 1) lock symbol FIRST (global lock order: symbol -> registry -> books)
    std::unique_lock<std::shared_mutex> symwLock(*get_or_create_symbol_mutex(req.symbol));

    // 2) create order
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

    if (o.type == OrderType::Market && o.tif == TimeInForce::GTC) {
        o.tif = TimeInForce::IOC;
    }

    const OrderId id = o.orderId;

    // 3) put into registry under symbol lock (prevents races with cancel/modify/cleanup)
    {
        std::unique_lock<std::shared_mutex> regwLock(registryMutex_);
        ordersRegistry_.emplace(id, std::move(ptr));
    }

    // 4) submit to book (book mutations protected by symbol lock)
    OrderBook& book = get_or_create_book(o.symbol);
    std::vector<Trade> trades = book.submit_order(o);

    // 5) if order is not GTC and still has remaining, erase from registry
    if (o.tif != TimeInForce::GTC && o.remaining > 0) {
        std::unique_lock<std::shared_mutex> regwLock(registryMutex_);
        ordersRegistry_.erase(id);
    }

    // 6) clean registry for fully-filled orders referenced by trades
    if (!trades.empty()) {
        std::unique_lock<std::shared_mutex> regwLock(registryMutex_);
        clean_registry(trades); // assumes clean_registry expects registry lock held
    }

    // 7) assign trade metadata
    if (!trades.empty()) {
        const auto ts = clock_.now();
        for (auto& t : trades) {
            t.tradeId   = tradeIdGenerator_.next();
            t.timestamp = ts;
        }
    }

    // 8) unlock symbol before callbacks/external sinks (optional but good to avoid re-entrancy deadlocks)
    symwLock.unlock();

    if (!trades.empty()) {
        tradeRepo_.add_trades(trades);
        on_trades(trades);
    }

    return id;
}

bool MatchingEngine::cancel_order(OrderId orderId) 
{
   // 1) find symbol (read-only)
    Symbol sym;
    {
        std::shared_lock<std::shared_mutex> regrLock(registryMutex_);
        auto it = ordersRegistry_.find(orderId);
        if (it == ordersRegistry_.end()) return false;
        sym = it->second->symbol;
    }

    // 2) lock symbol FIRST
    std::unique_lock<std::shared_mutex> symwLock(*get_or_create_symbol_mutex(sym));

    // 3) re-fetch the order pointer under registry lock (avoid dangling pointer)
    Order* optr = nullptr;
    {
        std::shared_lock<std::shared_mutex> regrLock(registryMutex_);
        auto it = ordersRegistry_.find(orderId);
        if (it == ordersRegistry_.end()) return false;
        optr = it->second.get();
    }

    // 4) cancel in book under symbol lock
    OrderBook& book = get_or_create_book(sym);
    bool removed = book.cancel_order(*optr);

    // 5) update registry
    if (removed) {
        std::unique_lock<std::shared_mutex> regwLock(registryMutex_);
        ordersRegistry_.erase(orderId);
        return true;
    }

    // Not removed: likely already fully filled / not on book anymore.
    // Clean registry if remaining==0.
    {
        std::unique_lock<std::shared_mutex> regwLock(registryMutex_);
        auto it = ordersRegistry_.find(orderId);
        if (it != ordersRegistry_.end() && it->second->remaining == 0) {
            ordersRegistry_.erase(it);
        }
    }
    return false;
}

bool MatchingEngine::modify_order(OrderId orderId, const ModifyOrderRequest& req) 
{
    // 1) get symbol + snapshot for validation
    Symbol sym;
    Order snapshot;
    {
        std::shared_lock<std::shared_mutex> regrLock(registryMutex_);
        auto it = ordersRegistry_.find(orderId);
        if (it == ordersRegistry_.end()) return false;
        sym = it->second->symbol;
        snapshot = *it->second;
    }

    // validate using snapshot (no symbol lock yet)
    auto vr = validate_modify_order(snapshot, req);
    if (vr != orderbook::RejectReason::None) return false;

    // 2) lock symbol FIRST
    std::unique_lock<std::shared_mutex> symwLock(*get_or_create_symbol_mutex(sym));

    // 3) re-fetch live pointer (order might have changed since snapshot)
    Order* optr = nullptr;
    {
        std::shared_lock<std::shared_mutex> regrLock(registryMutex_);
        auto it = ordersRegistry_.find(orderId);
        if (it == ordersRegistry_.end()) return false;
        optr = it->second.get();
    }

    // Re-validate quickly against live state (optional but safer)
    vr = validate_modify_order(*optr, req);
    if (vr != orderbook::RejectReason::None) return false;

    OrderBook& book = get_or_create_book(sym);

    // 4) decide whether rematching is needed
    const bool priceChanged = req.hasNewPrice;
    const Price newPrice = priceChanged ? req.newPrice : optr->price;

    bool willRematch = (optr->type == OrderType::Market);

    if (!willRematch && priceChanged) {
        const OrderBookSide& opposite = (optr->side == Side::Buy) ? book.asks() : book.bids();
        const PriceLevel* best = opposite.best_level();
        if (best) {
            const Price bestPrice = best->price();
            if (optr->side == Side::Buy) {
                if (newPrice >= bestPrice) willRematch = true;
            } else {
                if (newPrice <= bestPrice) willRematch = true;
            }
        }
    }

    // 5) if no rematch needed, do in-book modify and return
    if (!willRematch) {
        return book.modify_order(*optr, req);
    }

    // 6) build a temp order for FOK pre-check and new params
    Order temp = *optr;
    if (req.hasNewPrice)    temp.price = req.newPrice;
    if (req.hasNewQuantity) temp.qty   = req.newQuantity;
    temp.remaining = temp.qty - temp.filled;

    if (temp.tif == TimeInForce::FOK) {
        const OrderBookSide& opposite = (temp.side == Side::Buy) ? book.asks() : book.bids();
        const Quantity avail = opposite.available_quantity_for_order(temp);
        if (avail < temp.remaining) {
            return false;
        }
    }

    // 7) cancel old order from book
    const bool removed = book.cancel_order(*optr);
    if (!removed) {
        // if already fully filled, remove from registry; otherwise treat as failure
        std::unique_lock<std::shared_mutex> regwLock(registryMutex_);
        auto it = ordersRegistry_.find(orderId);
        if (it != ordersRegistry_.end() && it->second->remaining == 0) {
            ordersRegistry_.erase(it);
        }
        return false;
    }

    // 8) apply new fields to live order, resubmit
    optr->price     = temp.price;
    optr->qty       = temp.qty;
    optr->remaining = temp.remaining;

    std::vector<Trade> trades = book.submit_order(*optr);

    // 9) clean registry for filled orders referenced by trades
    if (!trades.empty()) {
        std::unique_lock<std::shared_mutex> regwLock(registryMutex_);
        clean_registry(trades);
    }

    // 10) assign trade metadata
    if (!trades.empty()) {
        const auto ts = clock_.now();
        for (auto& t : trades) {
            t.tradeId   = tradeIdGenerator_.next();
            t.timestamp = ts;
        }
    }

    // 11) unlock symbol before external calls
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

std::shared_mutex* MatchingEngine::get_or_create_symbol_mutex(const Symbol& symbol) {
    std::lock_guard<std::mutex> lock(symbolMutexesGuard_);
    auto it = symbolMutexes_.find(symbol);
    if (it != symbolMutexes_.end()) return it->second.get();
    
    // create new mutex for this symbol
    auto& ptr = symbolMutexes_[symbol];
    if (!ptr) ptr = std::make_unique<std::shared_mutex>();
    return ptr.get();
}

} 