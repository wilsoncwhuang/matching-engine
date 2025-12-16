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

    std::unique_lock<std::mutex> symLock(*get_or_create_symbol_mutex(req.symbol));

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

    {
        std::lock_guard<std::mutex> regLock(registryMutex_);
        ordersRegistry_.emplace(id, std::move(ptr));
    }

    OrderBook& book = get_or_create_book(o.symbol);
    std::vector<Trade> trades = book.submit_order(o);

    if (o.tif != TimeInForce::GTC && o.remaining > 0) {
        std::lock_guard<std::mutex> regLock(registryMutex_);
        ordersRegistry_.erase(id);
    }

    if (!trades.empty()) {
        clean_registry(trades);
    }

    if (!trades.empty()) {
        const auto ts = clock_.now();
        for (auto& t : trades) {
            t.tradeId   = tradeIdGenerator_.next();
            t.timestamp = ts;
        }
    }

    symLock.unlock();

    if (!trades.empty()) {
        tradeRepo_.add_trades(trades);
        on_trades(trades);
    }

    return id;
}

bool MatchingEngine::cancel_order(OrderId orderId)
{
    Symbol sym;
    {
        std::lock_guard<std::mutex> regLock(registryMutex_);
        auto it = ordersRegistry_.find(orderId);
        if (it == ordersRegistry_.end()) return false;
        sym = it->second->symbol;
    }

    std::unique_lock<std::mutex> symLock(*get_or_create_symbol_mutex(sym));

    Order* optr = nullptr;
    {
        std::lock_guard<std::mutex> regLock(registryMutex_);
        auto it = ordersRegistry_.find(orderId);
        if (it == ordersRegistry_.end()) return false;
        optr = it->second.get();
    }

    OrderBook& book = get_or_create_book(sym);
    const bool removed = book.cancel_order(*optr);

    if (removed) {
        std::lock_guard<std::mutex> regLock(registryMutex_);
        ordersRegistry_.erase(orderId);
        return true;
    }

    {
        std::lock_guard<std::mutex> regLock(registryMutex_);
        auto it = ordersRegistry_.find(orderId);
        if (it != ordersRegistry_.end() && it->second->remaining == 0) {
            ordersRegistry_.erase(it);
        }
    }
    return false;
}

bool MatchingEngine::modify_order(OrderId orderId, const ModifyOrderRequest& req)
{
    Symbol sym;
    Order snapshot;
    {
        std::lock_guard<std::mutex> regLock(registryMutex_);
        auto it = ordersRegistry_.find(orderId);
        if (it == ordersRegistry_.end()) return false;
        sym = it->second->symbol;
        snapshot = *it->second;
    }

    auto vr = validate_modify_order(snapshot, req);
    if (vr != orderbook::RejectReason::None) return false;

    std::unique_lock<std::mutex> symLock(*get_or_create_symbol_mutex(sym));

    Order* optr = nullptr;
    {
        std::lock_guard<std::mutex> regLock(registryMutex_);
        auto it = ordersRegistry_.find(orderId);
        if (it == ordersRegistry_.end()) return false;
        optr = it->second.get();
    }

    vr = validate_modify_order(*optr, req);
    if (vr != orderbook::RejectReason::None) return false;

    OrderBook& book = get_or_create_book(sym);

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

    if (!willRematch) {
        return book.modify_order(*optr, req);
    }

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

    const bool removed = book.cancel_order(*optr);
    if (!removed) {
        std::lock_guard<std::mutex> regLock(registryMutex_);
        auto it = ordersRegistry_.find(orderId);
        if (it != ordersRegistry_.end() && it->second->remaining == 0) {
            ordersRegistry_.erase(it);
        }
        return false;
    }

    optr->price     = temp.price;
    optr->qty       = temp.qty;
    optr->remaining = temp.remaining;

    std::vector<Trade> trades = book.submit_order(*optr);

    if (!trades.empty()) {
        clean_registry(trades);
    }

    if (!trades.empty()) {
        const auto ts = clock_.now();
        for (auto& t : trades) {
            t.tradeId   = tradeIdGenerator_.next();
            t.timestamp = ts;
        }
    }

    symLock.unlock();

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
    std::lock_guard<std::mutex> lock(booksMutex_);
    auto& slot = books_[symbol];
    if (!slot) slot = std::make_unique<OrderBook>();
    return *slot;
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
    std::lock_guard<std::mutex> regLock(registryMutex_);

    std::unordered_set<OrderId> ids;
    for (const auto& t : trades) {
        if (t.buyOrderId  != orderbook::INVALID_ORDER_ID) ids.insert(t.buyOrderId);
        if (t.sellOrderId != orderbook::INVALID_ORDER_ID) ids.insert(t.sellOrderId);
    }

    for (auto id : ids) {
        auto it = ordersRegistry_.find(id);
        if (it != ordersRegistry_.end() && it->second->remaining == 0) {
            ordersRegistry_.erase(it);
        }
    }
}

Symbol MatchingEngine::get_symbol_by_order(OrderId orderId) const
{
    std::lock_guard<std::mutex> regLock(registryMutex_);
    auto it = ordersRegistry_.find(orderId);
    if (it != ordersRegistry_.end()) return it->second->symbol;
    return "";
}

std::mutex* MatchingEngine::get_or_create_symbol_mutex(const Symbol& symbol)
{
    std::lock_guard<std::mutex> lock(symbolMutexesGuard_);
    auto it = symbolMutexes_.find(symbol);
    if (it != symbolMutexes_.end()) return it->second.get();

    auto& ptr = symbolMutexes_[symbol];
    if (!ptr) ptr = std::make_unique<std::mutex>();
    return ptr.get();
}

}
