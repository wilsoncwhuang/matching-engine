#ifndef MODIFY_ORDER_REQUEST_HPP
#define MODIFY_ORDER_REQUEST_HPP

#include <cstdint>
#include "orderbook/types.hpp"

namespace orderbook::api {

struct ModifyOrderRequest {
    bool     hasNewQuantity{false};
    bool     hasNewPrice{false};

    Quantity newQuantity{0};
    Price    newPrice{0.0};

    ModifyOrderRequest() = default;

    explicit ModifyOrderRequest(Quantity newQty)
        : hasNewQuantity{true}
        , hasNewPrice{false}
        , newQuantity{newQty}
        , newPrice{0.0}
    {
    }

    explicit ModifyOrderRequest(Price newPx)
        : hasNewQuantity{false}
        , hasNewPrice{true}
        , newQuantity{0}
        , newPrice{newPx}
    {
    }

    ModifyOrderRequest(Quantity newQty, Price newPx)
        : hasNewQuantity{true}
        , hasNewPrice{true}
        , newQuantity{newQty}
        , newPrice{newPx}
    {
    }
};

}

#endif