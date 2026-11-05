//
// Created by sam on 04/11/2026.
//

#ifndef ATTACK_LAB_BFT_NODE_H
#define ATTACK_LAB_BFT_NODE_H
#include <cstdint>

#include "protocol/message.h"

class Node
{
public:
    virtual void onMessage(const Message& m) = 0;
    virtual void onTimer(std::uint64_t timerId) = 0;

    virtual ~Node() = default;
};

#endif //ATTACK_LAB_BFT_NODE_H
