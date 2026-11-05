//
// Created by sam on 05/11/2026.
//

#ifndef ATTACK_LAB_BFT_SIMPOLICY_H
#define ATTACK_LAB_BFT_SIMPOLICY_H
#include <random>

#include "deliveryPolicy.h"

enum class PacketStatus
{
    Normal,
    Dropped,
    AddedDelay,
    Duplicated,
    DuplicatedWithDelay
};

// Policy with random seed
class SimPolicy: public DeliveryPolicy
{
private:
    std::mt19937_64 randomGenerator;

public:
    explicit SimPolicy(std::uint64_t seed);
    std::vector<std::uint64_t> decide(const Message&, std::uint64_t) override;
};


#endif //ATTACK_LAB_BFT_SIMPOLICY_H
