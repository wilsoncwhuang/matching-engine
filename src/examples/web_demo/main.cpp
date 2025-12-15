#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <chrono>

#include "crow/crow_all.h"

#include "orderbook/util/simulated_clock.hpp"
#include "orderbook/util/system_clock.hpp"
#include "orderbook/core/matching_engine.hpp"
#include "orderbook/core/order_book.hpp"
#include "orderbook/report/internal_trade_repository.hpp"
#include "orderbook/report/report_service.hpp"

// Global state for demo
struct DemoState {
    std::vector<std::string> commands;
    size_t currentStep = 0;
    
    std::unique_ptr<orderbook::util::SimulatedClock> clock;  // For trading simulation
    std::unique_ptr<orderbook::util::SystemClock> systemClock;  // For recording trade timestamps
    std::unique_ptr<orderbook::report::InternalTradeRepository> tradeRepo;
    std::unique_ptr<orderbook::core::MatchingEngine> engine;
    std::unique_ptr<orderbook::report::ReportService> reportService;
    
    std::vector<orderbook::core::Trade> allTrades;
    std::map<orderbook::TradeId, double> tradeTimestamps;  // Map trade ID to system time
} gState;

bool parse_side(const std::string& s, orderbook::Side& out) {
    if (s == "BUY" || s == "buy") { out = orderbook::Side::Buy; return true; }
    if (s == "SELL" || s == "sell") { out = orderbook::Side::Sell; return true; }
    return false;
}

bool parse_order_type(const std::string& s, orderbook::OrderType& out) {
    if (s == "LIMIT" || s == "limit") { out = orderbook::OrderType::Limit; return true; }
    if (s == "MARKET" || s == "market") { out = orderbook::OrderType::Market; return true; }
    return false;
}

bool parse_tif(const std::string& s, orderbook::TimeInForce& out) {
    if (s == "GTC" || s == "gtc") { out = orderbook::TimeInForce::GTC; return true; }
    if (s == "IOC" || s == "ioc") { out = orderbook::TimeInForce::IOC; return true; }
    if (s == "FOK" || s == "fok") { out = orderbook::TimeInForce::FOK; return true; }
    return false;
}

std::string orderbook_to_json(const orderbook::core::OrderBook& book) {
    std::ostringstream json;
    json << "{";
    
    // Get bids 
    const auto& bidSide = book.bids();
    auto bidLevels = bidSide.top_k_levels(5);
    json << "\"bids\":[";
    bool firstBid = true;
    for (const auto* level : bidLevels) {
        if (!level) continue;
        if (!firstBid) json << ",";
        json << "{\"price\":" << level->price() 
             << ",\"quantity\":" << level->volume()
             << ",\"orders\":" << level->size() << "}";
        firstBid = false;
    }
    json << "],";
    
    // Get asks 
    const auto& askSide = book.asks();
    auto askLevels = askSide.top_k_levels(5);
    json << "\"asks\":[";
    bool firstAsk = true;
    for (const auto* level : askLevels) {
        if (!level) continue;
        if (!firstAsk) json << ",";
        json << "{\"price\":" << level->price()
             << ",\"quantity\":" << level->volume()
             << ",\"orders\":" << level->size() << "}";
        firstAsk = false;
    }
    json << "]}";
    
    return json.str();
}

void load_commands(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open commands file: " << filename << std::endl;
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
        gState.commands.push_back(line);
    }
    
    std::cout << "Loaded " << gState.commands.size() << " commands\n";
}

void init_engine() {
    gState.clock = std::make_unique<orderbook::util::SimulatedClock>();
    gState.systemClock = std::make_unique<orderbook::util::SystemClock>();
    gState.tradeRepo = std::make_unique<orderbook::report::InternalTradeRepository>();
    gState.engine = std::make_unique<orderbook::core::MatchingEngine>(*gState.clock, *gState.tradeRepo);
    gState.reportService = std::make_unique<orderbook::report::ReportService>(*gState.tradeRepo);
    
    gState.engine->register_trade_listener([](const std::vector<orderbook::core::Trade>& trades) {
        double secs = gState.systemClock->now().value().time_since_epoch().count() / 1e9;
        for (const auto& trade : trades) {
            gState.tradeTimestamps[trade.tradeId] = secs;
        }
        gState.allTrades.insert(gState.allTrades.end(), trades.begin(), trades.end());
    });
}

int main() {
    // Initialize
    init_engine();
    load_commands("cases.txt");
    
    crow::SimpleApp app;
    
    // Serve index.html
    CROW_ROUTE(app, "/")([](){
        std::ifstream file("static/index.html");
        if (file.is_open()) {
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            return crow::response(content);
        }
        return crow::response(404, "index.html not found");
    });
    
    // Serve static CSS
    CROW_ROUTE(app, "/style.css")([](){
        std::ifstream file("static/style.css");
        if (file.is_open()) {
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            crow::response res(content);
            res.set_header("Content-Type", "text/css");
            return res;
        }
        return crow::response(404, "style.css not found");
    });
    
    // Serve static JS
    CROW_ROUTE(app, "/app.js")([](){
        std::ifstream file("static/app.js");
        if (file.is_open()) {
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            crow::response res(content);
            res.set_header("Content-Type", "application/javascript");
            return res;
        }
        return crow::response(404, "app.js not found");
    });
    
    CROW_ROUTE(app, "/api/reset").methods(crow::HTTPMethod::Post)([](const crow::request&){
        gState.currentStep = 0;
        gState.allTrades.clear();
        gState.tradeTimestamps.clear();
        init_engine();
        
        crow::json::wvalue result;
        result["status"] = "success";
        result["message"] = "Demo reset";
        return crow::response(result);
    });
    
    CROW_ROUTE(app, "/api/step").methods(crow::HTTPMethod::Post)([](const crow::request& req){
        crow::json::wvalue result;
        
        if (gState.currentStep >= gState.commands.size()) {
            result["status"] = "completed";
            result["message"] = "All steps completed";
            return crow::response(result);
        }
        
        std::string cmd = gState.commands[gState.currentStep];
        std::istringstream iss(cmd);
        std::string action;
        iss >> action;
        
        // Get current symbol from query parameter
        auto symbol_param = req.url_params.get("symbol");
        std::string current_symbol = symbol_param ? symbol_param : "AAPL";
        
        result["command"] = cmd;
        result["step"] = gState.currentStep;
        result["total_steps"] = gState.commands.size();
        
        try {
            if (action == "new") {
                std::string symbol, sideStr, typeStr, tifStr;
                orderbook::Price price;
                orderbook::Quantity qty;
                
                iss >> symbol >> sideStr >> typeStr >> tifStr >> price >> qty;
                
                orderbook::Side side;
                orderbook::OrderType type;
                orderbook::TimeInForce tif;
                
                if (!parse_side(sideStr, side) || !parse_order_type(typeStr, type) || !parse_tif(tifStr, tif)) {
                    result["status"] = "error";
                    result["message"] = "Invalid order parameters";
                    return crow::response(result);
                }
                
                orderbook::api::NewOrderRequest req;
                req.symbol = symbol;
                req.side = side;
                req.type = type;
                req.tif = tif;
                req.price = price;
                req.quantity = qty;
                
                orderbook::OrderId orderId = gState.engine->new_order(req);
                
                result["status"] = "success";
                result["action"] = "new_order";
                result["order_id"] = orderId;
                result["symbol"] = symbol;
                result["side"] = sideStr;
                result["price"] = price;
                result["quantity"] = qty;
                
                // Get orderbook
                auto& book = gState.engine->get_or_create_book(symbol);
                result["orderbook"] = crow::json::load(orderbook_to_json(book));
                result["current_symbol"] = symbol;
                
            } 
            else if (action == "cancel") {
                std::string orderId_str;
                iss >> orderId_str;
                
                orderbook::OrderId orderId = std::stoull(orderId_str);
                
                // Get symbol BEFORE canceling
                std::string symbol = gState.engine->get_symbol_by_order(orderId);
                if (symbol.empty()) symbol = current_symbol;
                
                bool success = gState.engine->cancel_order(orderId);
                
                result["status"] = success ? "success" : "error";
                result["action"] = "cancel";
                result["order_id"] = orderId;
                
                auto& book = gState.engine->get_or_create_book(symbol);
                result["orderbook"] = crow::json::load(orderbook_to_json(book));
                result["current_symbol"] = symbol;
                
            } 
            else if (action == "modify") {
                std::string orderId_str;
                orderbook::Quantity newQty;
                orderbook::Price newPrice;
                
                iss >> orderId_str >> newQty >> newPrice;
                
                orderbook::OrderId orderId = std::stoull(orderId_str);
                
                // Get symbol BEFORE modifying
                std::string symbol = gState.engine->get_symbol_by_order(orderId);
                if (symbol.empty()) symbol = current_symbol;
                
                orderbook::api::ModifyOrderRequest req;
                req.hasNewQuantity = true;
                req.hasNewPrice = true;
                req.newQuantity = newQty;
                req.newPrice = newPrice;
                
                bool success = gState.engine->modify_order(orderId, req);
                
                result["status"] = success ? "success" : "error";
                result["action"] = "modify";
                result["order_id"] = orderId;
                
                auto& book = gState.engine->get_or_create_book(symbol);
                result["orderbook"] = crow::json::load(orderbook_to_json(book));
                result["current_symbol"] = symbol;
                
            }
            
            // Get recent trades for current symbol only
            std::vector<crow::json::wvalue> trades;
            for (const auto& trade : gState.allTrades) {
                if (trade.symbol == current_symbol) {
                    crow::json::wvalue t;
                    t["trade_id"] = trade.tradeId;
                    t["price"] = trade.price;
                    t["quantity"] = trade.quantity;
                    t["buy_order_id"] = trade.buyOrderId;
                    t["sell_order_id"] = trade.sellOrderId;
                    // Get system time when trade occurred
                    auto it = gState.tradeTimestamps.find(trade.tradeId);
                    if (it != gState.tradeTimestamps.end()) {
                        t["timestamp"] = it->second;
                    }
                    trades.push_back(std::move(t));
                }
            }
            // Keep only last 5 trades
            if (trades.size() > 5) {
                trades.erase(trades.begin(), trades.end() - 5);
            }
            result["recent_trades"] = std::move(trades);
            result["total_trades"] = gState.allTrades.size();
            
            // Add volume stats for current symbol
            auto volumeReport = gState.reportService->volume_all(current_symbol);
            auto vstat = volumeReport.stats();
            result["total_volume"] = static_cast<long long>(vstat.totalQuantity);
            
            // Add price stats for current symbol
            auto priceReport = gState.reportService->price_all(current_symbol);
            auto pstats = priceReport.stats();
            result["avg_price"] = pstats.avgPrice;
            result["min_price"] = pstats.minPrice;
            result["max_price"] = pstats.maxPrice;
            result["price_std"] = pstats.stdDevPct;
        } 
        catch (const std::exception& e) {
            result["status"] = "error";
            result["message"] = e.what();
        }
        
        gState.currentStep++;
        
        return crow::response(result);
    });
    
    // Get current state
    CROW_ROUTE(app, "/api/state")([](const crow::request& req){
        auto symbol_param = req.url_params.get("symbol");
        std::string symbol = symbol_param ? symbol_param : "AAPL";
        
        crow::json::wvalue result;
        result["current_step"] = gState.currentStep;
        result["total_steps"] = gState.commands.size();
        result["total_trades"] = gState.allTrades.size();
        
        if (gState.engine) {
            auto& book = gState.engine->get_or_create_book(symbol);
            result["orderbook"] = crow::json::load(orderbook_to_json(book));
        }
        
        // Add volume stats for current symbol
        auto volumeReport = gState.reportService->volume_all(symbol);
        auto vstat = volumeReport.stats();
        result["total_volume"] = static_cast<long long>(vstat.totalQuantity);
        
        // Add price stats for current symbol
        auto priceReport = gState.reportService->price_all(symbol);
        auto pstats = priceReport.stats();
        result["avg_price"] = pstats.avgPrice;
        result["min_price"] = pstats.minPrice;
        result["max_price"] = pstats.maxPrice;
        result["price_std"] = pstats.stdDevPct;
        
        // Add recent trades for current symbol
        std::vector<crow::json::wvalue> trades;
        for (const auto& trade : gState.allTrades) {
            if (trade.symbol == symbol) {
                crow::json::wvalue t;
                t["trade_id"] = trade.tradeId;
                t["price"] = trade.price;
                t["quantity"] = trade.quantity;
                t["buy_order_id"] = trade.buyOrderId;
                t["sell_order_id"] = trade.sellOrderId;
                // Get system time when trade occurred
                auto it = gState.tradeTimestamps.find(trade.tradeId);
                if (it != gState.tradeTimestamps.end()) {
                    t["timestamp"] = it->second;
                }
                trades.push_back(std::move(t));
            }
        }
        // Keep only last 5 trades
        if (trades.size() > 5) {
            trades.erase(trades.begin(), trades.end() - 5);
        }
        result["recent_trades"] = std::move(trades);
        
        return crow::response(result);
    });
    
    std::cout << "Starting web server on http://localhost:8080\n";
    std::cout << "Open your browser to see the demo\n";
    
    app.port(8080).run();
    
    return 0;
}
