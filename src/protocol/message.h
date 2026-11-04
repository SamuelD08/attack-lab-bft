//
// Created by sam on 04/11/2026.
//

#ifndef ATTACK_LAB_BFT_MESSAGE_H
#define ATTACK_LAB_BFT_MESSAGE_H
#include <array>
#include <cstdint>
#include <sodium.h>
#include <vector>

#include "kv/kv_store.h"

enum class MessageType
{
    REQUEST,
    PRE_PREPARE,
    PREPARE,
    COMMIT,
    REPLY
};

struct Message
{
    std::uint32_t sender = 0;
    std::uint32_t receiver = 0;
    MessageType type = MessageType::REQUEST;
    std::uint64_t view = 0;
    std::uint64_t seq = 0;
    Digest digest{};
    Request payload;
    Result result;
    std::array<unsigned char, crypto_sign_BYTES> signature{};

    bool operator==(const Message&) const = default;
};

std::vector<unsigned char> bytesToSign(const Message& message);
std::vector<unsigned char> serialize(const Message& message);



#endif //ATTACK_LAB_BFT_MESSAGE_H
