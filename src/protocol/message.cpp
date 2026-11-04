//
// Created by sam on 04/11/2026.
//

#include "protocol/message.h"

//Helpers
namespace
{
    void appendU64(std::vector<unsigned char>& out, std::uint64_t value)
    {
        const auto* p = reinterpret_cast<const unsigned char*>(&value);
        out.insert(out.end(), p, p + sizeof(value));
    }
    void appendU32(std::vector<unsigned char>& out, std::uint32_t value)
    {
        const auto* p = reinterpret_cast<const unsigned char*>(&value);
        out.insert(out.end(), p, p + sizeof(value));
    }
    void appendU8(std::vector<unsigned char>& out, std::uint8_t value)
    {
        const auto* p = reinterpret_cast<const unsigned char*>(&value);
        out.insert(out.end(), p, p + sizeof(value));
    }
    void appendBytes(std::vector<unsigned char>& out, const std::string& s)
    {
        appendU64(out, s.size());
        for (auto element : s)
        {
            out.insert(out.end(), &element, &element + sizeof(element));
        }
    }
}

/*
 * every field except the signature.
 */
std::vector<unsigned char> bytesToSign(const Message& message)
{
    std::vector<unsigned char> message_serialized;
    appendU32(message_serialized, message.sender);
    appendU32(message_serialized, message.receiver);
    appendU8(message_serialized, static_cast<std::uint8_t>(message.type));
    appendU64(message_serialized, message.view);
    appendU64(message_serialized, message.seq);
    message_serialized.insert(message_serialized.end(), message.digest.begin(), message.digest.end());
    // Request
    appendU32(message_serialized, message.payload.clientId);
    appendU64(message_serialized, message.payload.requestId);
    appendU8(message_serialized, static_cast<std::uint8_t>(message.payload.op));
    appendBytes(message_serialized, message.payload.key);
    appendBytes(message_serialized, message.payload.value);
    // Result
    appendU8(message_serialized, static_cast<std::uint8_t>(message.result.found));
    appendBytes(message_serialized, message.result.value);
    return message_serialized;
}

/*
 * bytesToSign(message) plus the 64 signature bytes.
 */
std::vector<unsigned char> serialize(const Message& message)
{
    std::vector<unsigned char> message_serialized = bytesToSign(message);
    message_serialized.insert(message_serialized.end(), message.signature.begin(), message.signature.end());
    return message_serialized;
}
