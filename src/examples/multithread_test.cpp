#include <atomic>
#include <chrono>
#include <mutex>
#include <random>
#include <thread>
#include <vector>
#include <iostream>

#include "orderbook/core/matching_engine.hpp"
#include "orderbook/api/new_order_request.hpp"
#include "orderbook/api/modify_order_request.hpp"

#include "orderbook/util/simulated_clock.hpp"
#include "orderbook/report/internal_trade_repository.hpp"

using namespace orderbook;
using namespace orderbook::core;
using namespace orderbook::api;
using namespace orderbook::util;
using namespace orderbook::report;

int main() {
    SimulatedClock clock;
    InternalTradeRepository repo;
    MatchingEngine eng(clock, repo);

    const Symbol sym = "AAPL";

    std::mutex ids_mtx;
    std::vector<OrderId> live_ids;
    
    std::atomic<bool> stop{false};

    auto worker = [&](int tid) {
        std::mt19937_64 rng((uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count() ^ (uint64_t)tid);
        std::uniform_int_distribution<int> opdist(0, 99);
        std::uniform_int_distribution<int> sidedist(0, 1);
        std::uniform_int_distribution<int> qtydist(1, 50);
        std::uniform_real_distribution<double> pricedist(90.0, 110.0);

        while (!stop.load(std::memory_order_relaxed)) {
            int op = opdist(rng);

            if (op < 55) {
                // 55% new order
                NewOrderRequest req;
                req.symbol   = sym;
                req.side     = (sidedist(rng) == 0) ? Side::Buy : Side::Sell;
                req.type     = OrderType::Limit;
                req.tif      = TimeInForce::GTC;
                req.price    = pricedist(rng);
                req.quantity = qtydist(rng);

                OrderId id = eng.new_order(req);
                if (id != INVALID_ORDER_ID) {
                    std::lock_guard<std::mutex> lk(ids_mtx);
                    live_ids.push_back(id);
                }
            } 
            else if (op < 80) {
                // 25% cancel
                OrderId id = INVALID_ORDER_ID;
                {
                    std::lock_guard<std::mutex> lk(ids_mtx);
                    if (!live_ids.empty()) {
                        id = live_ids[rng() % live_ids.size()];
                    }
                }
                if (id != INVALID_ORDER_ID) {
                    eng.cancel_order(id);
                }
            } 
            else {
                // 20% modify
                OrderId id = INVALID_ORDER_ID;
                {
                    std::lock_guard<std::mutex> lk(ids_mtx);
                    if (!live_ids.empty()) {
                        id = live_ids[rng() % live_ids.size()];
                    }
                }
                if (id != INVALID_ORDER_ID) {
                    ModifyOrderRequest mreq;
                    // randomly modify qty or price
                    if ((rng() & 1) == 0) {
                        mreq.hasNewQuantity = true;
                        mreq.newQuantity = qtydist(rng);
                    } else {
                        mreq.hasNewPrice = true;
                        mreq.newPrice = pricedist(rng);
                    }
                    eng.modify_order(id, mreq);
                }
            }
        }
    };

    const int num_threads = 8;
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (int i = 0; i < num_threads; ++i) threads.emplace_back(worker, i);

    std::this_thread::sleep_for(std::chrono::seconds(10));
    stop.store(true);

    for (auto& t : threads) t.join();

    std::cout << "stress test done\n";
    return 0;
}
