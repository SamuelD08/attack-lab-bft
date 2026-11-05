//
// Created by sam on 05/11/2026.
//

#include "simPolicy.h"

SimPolicy::SimPolicy(const std::uint64_t seed): randomGenerator(seed){}

std::vector<std::uint64_t> SimPolicy::decide(const Message&, std::uint64_t)
{
    // Choose a random status
    std::uniform_int_distribution<int> distrib_status(0, 4);
    const auto status = static_cast<PacketStatus>(distrib_status(randomGenerator));

    std::uniform_int_distribution<int> distrib_added_delay(0, 25);

    std::vector<std::uint64_t> returnValue;

    switch (status)
    {
    case PacketStatus::Normal:
        // {0}
        returnValue.push_back(0);
        break;
    case PacketStatus::AddedDelay:
        // {addedDelay}
        returnValue.push_back(distrib_added_delay(randomGenerator));
        break;
    case PacketStatus::Dropped:
        // {}
        break;
    case PacketStatus::Duplicated:
        // {0, 0}
        returnValue.push_back(0);
        returnValue.push_back(0);
        break;
    case PacketStatus::DuplicatedWithDelay:
        // {0, addedDelay}
        returnValue.push_back(0);
        returnValue.push_back(distrib_added_delay(randomGenerator));
        break;
    }
    return returnValue;
}
