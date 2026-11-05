//
// Created by sam on 05/11/2026.
//

#ifndef ATTACK_LAB_BFT_DELIVERYPOLICY_H
#define ATTACK_LAB_BFT_DELIVERYPOLICY_H

#include "protocol/message.h"
#include <vector>

class DeliveryPolicy
{
public:
    virtual std::vector<std::uint64_t> decide(const Message& message, std::uint64_t now) = 0;

    virtual ~DeliveryPolicy() = default;
};


#endif //ATTACK_LAB_BFT_DELIVERYPOLICY_H
