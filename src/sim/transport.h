//
// Created by sam on 04/11/2026.
//

#ifndef ATTACK_LAB_BFT_TRANSPORT_H
#define ATTACK_LAB_BFT_TRANSPORT_H
#include "protocol/message.h"

class Transport
{
public:
    virtual void send(const Message& m) = 0;
    virtual void setTimer(std::uint32_t nodeId, std::uint64_t delay, std::uint64_t timerId) = 0; // in microseconds
    [[nodiscard]] virtual std::uint64_t now() const = 0;

    virtual ~Transport() = default;
};

#endif //ATTACK_LAB_BFT_TRANSPORT_H
