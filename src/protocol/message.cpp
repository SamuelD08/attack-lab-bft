//
// Created by sam on 04/11/2026.
//

#include "protocol/message.h"

#include <cstring>

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

    void appendRequest(std::vector<unsigned char>& out, const Request& request)
    {
        appendU32(out, request.clientId);
        appendU64(out, request.requestId);
        appendU8(out, static_cast<std::uint8_t>(request.op));
        appendBytes(out, request.key);
        appendBytes(out, request.value);
    }

    struct Reader
    {
        const std::vector<unsigned char>& bytes;
        std::size_t pos = 0;

        [[nodiscard]] std::size_t remaining() const
        {
            return bytes.size()-pos;
        }

        bool read64U(std::uint64_t& out)
        {
            if (remaining() < sizeof(out)) return false;
            std::memcpy(&out, bytes.data() + pos, sizeof(out));
            pos += sizeof(out);
            return true;
        }

        bool read32U(std::uint32_t& out)
        {
            if (remaining() < sizeof(out)) return false;
            std::memcpy(&out, bytes.data() + pos, sizeof(out));
            pos += sizeof(out);
            return true;
        }

        bool read8U(std::uint8_t& out)
        {
            if (remaining() < sizeof(out)) return false;
            std::memcpy(&out, bytes.data() + pos, sizeof(out));
            pos += sizeof(out);
            return true;
        }


        bool readBytes(std::string& out)
        {
            std::uint64_t size;
            if (remaining() < sizeof(std::uint64_t)) return false;
            std::memcpy(&size, bytes.data() + pos, sizeof(std::uint64_t));
            pos += sizeof(std::uint64_t);

            if (remaining() < size) return false;
            out.assign(reinterpret_cast<const char*>(bytes.data() + pos), size);
            pos += size;

            return true;
        }

        template<std::size_t N>
        bool readFixed(std::array<unsigned char, N>& out)
        {
            if (remaining() < N) return false;
            std::memcpy(out.data(), bytes.data() + pos, N);
            pos += N;
            return true;
        }

    };
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
    appendRequest(message_serialized, message.payload);
    // Result
    appendU8(message_serialized, static_cast<std::uint8_t>(message.result.status));
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

std::optional<Message> parse(const std::vector<unsigned char>& bytes)
{
    Reader r(bytes);
    Message m;

    std::uint8_t type = 0;
    if (!r.read32U(m.sender) || !r.read32U(m.receiver) || !r.read8U(type)) return std::nullopt;
    if (type > static_cast<std::uint8_t>(MessageType::REPLY)) return std::nullopt;
    m.type = static_cast<MessageType>(type);

    if (!r.read64U(m.view) || !r.read64U(m.seq)) return std::nullopt;
    if (!r.readFixed(m.digest)) return std::nullopt;

    // Request
    std::uint8_t op = 0;
    if (!r.read32U(m.payload.clientId) || !r.read64U(m.payload.requestId) || !r.read8U(op)) return std::nullopt;
    if (op > static_cast<std::uint8_t>(Operation::Get)) return std::nullopt;
    m.payload.op = static_cast<Operation>(op);
    if (!r.readBytes(m.payload.key) || !r.readBytes(m.payload.value)) return std::nullopt;

    // Result
    std::uint8_t status = 0;
    if (!r.read8U(status)) return std::nullopt;
    if (status > static_cast<std::uint8_t>(Status::NotExecutedYet)) return std::nullopt;
    m.result.status = static_cast<Status>(status);
    if (!r.readBytes(m.result.value)) return std::nullopt;

    if (!r.readFixed(m.signature)) return std::nullopt;

    if (r.remaining() != 0) return std::nullopt;
    return m;

}

Digest requestDigest(const Request& request)
{
    // Request -> bytes
    std::vector<unsigned char> bytes;
    appendRequest(bytes, request);

    // Hash bytes
    Digest digest{};
    crypto_hash_sha256(digest.data(), bytes.data(), bytes.size());
    return digest;
}
