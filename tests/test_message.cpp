//
// Created by sam on 04/11/2026.
//
// Tests for Message, bytesToSign(), serialize() and parse().
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "protocol/message.h"

namespace
{
    // Byte positions in the serialized format (see bytesToSign()).
    constexpr std::size_t kTypeOffset = 8;       // sender (4) + receiver (4)
    constexpr std::size_t kOpOffset = 69;        // ... + view (8) + seq (8) + digest (32) + clientId (4) + requestId (8)
    constexpr std::size_t kKeyLengthOffset = 70; // right after op (1)

    // Status byte: after the key's length (8), the key, the value's length (8) and the value.
    std::size_t statusOffset(const Message& m)
    {
        return 86 + m.payload.key.size() + m.payload.value.size();
    }

    // A message with large, non-default values in every field.
    Message makeSample()
    {
        Message m;
        m.sender = 300;
        m.receiver = 2;
        m.type = MessageType::PRE_PREPARE;
        m.view = 0;
        m.seq = 257;
        m.digest.fill(0xAB);
        m.payload.clientId = 7;
        m.payload.requestId = 1000;
        m.payload.op = Operation::Put;
        m.payload.key = std::string(300, 'k');
        m.payload.value = "v";
        m.result.status = Status::Ok;
        m.result.value = "result";
        m.signature.fill(0x5A);
        return m;
    }

    // Overwrites 8 bytes of a serialized message with a length.
    void writeLength(std::vector<unsigned char>& bytes, std::size_t offset, std::uint64_t length)
    {
        std::memcpy(bytes.data() + offset, &length, sizeof length);
    }
}

// ---------- operator== ----------

TEST(MessageEquality, DefaultMessagesAreEqual)
{
    EXPECT_EQ(Message{}, Message{});
}

TEST(MessageEquality, ChangingOneFieldMakesMessagesDifferent)
{
    const Message original = makeSample();

    Message seq = original;
    seq.seq += 1;
    EXPECT_NE(seq, original);

    Message digest = original;
    digest.digest[0] ^= 1;
    EXPECT_NE(digest, original);

    Message key = original;
    key.payload.key += "x";
    EXPECT_NE(key, original);

    Message signature = original;
    signature.signature[0] ^= 1;
    EXPECT_NE(signature, original);
}

// ---------- bytesToSign ----------

TEST(BytesToSign, IsDeterministic)
{
    const Message m = makeSample();
    EXPECT_EQ(bytesToSign(m), bytesToSign(m));
}

TEST(BytesToSign, IgnoresTheSignature)
{
    const Message original = makeSample();
    Message changed = original;
    changed.signature[0] ^= 1;
    EXPECT_EQ(bytesToSign(changed), bytesToSign(original));
}

TEST(BytesToSign, IncludesEveryOtherField)
{
    const Message original = makeSample();
    const std::vector<unsigned char> originalBytes = bytesToSign(original);

    // Each change modifies exactly one field.
    const std::vector<std::pair<const char*, std::function<void(Message&)>>> changes = {
        {"sender", [](Message& m) { m.sender += 1; }},
        {"receiver", [](Message& m) { m.receiver += 1; }},
        {"type", [](Message& m) { m.type = MessageType::COMMIT; }},
        {"view", [](Message& m) { m.view += 1; }},
        {"seq", [](Message& m) { m.seq += 1; }},
        {"digest", [](Message& m) { m.digest[31] ^= 1; }},
        {"clientId", [](Message& m) { m.payload.clientId += 1; }},
        {"requestId", [](Message& m) { m.payload.requestId += 1; }},
        {"op", [](Message& m) { m.payload.op = Operation::Get; }},
        {"key", [](Message& m) { m.payload.key += "x"; }},
        {"value", [](Message& m) { m.payload.value += "x"; }},
        {"status", [](Message& m) { m.result.status = Status::NotFound; }},
        {"result value", [](Message& m) { m.result.value += "x"; }},
    };

    for (const auto& [field, change] : changes)
    {
        Message changed = original;
        change(changed);
        EXPECT_NE(bytesToSign(changed), originalBytes) << "field not signed: " << field;
    }
}

TEST(BytesToSign, PrepareAndCommitWithSameFieldsDiffer)
{
    Message prepare = makeSample();
    prepare.type = MessageType::PREPARE;
    Message commit = prepare;
    commit.type = MessageType::COMMIT;
    EXPECT_NE(bytesToSign(prepare), bytesToSign(commit));
}

TEST(BytesToSign, KeyAndValueBoundaryIsNotAmbiguous)
{
    Message a = makeSample();
    a.payload.key = "ab";
    a.payload.value = "c";
    Message b = makeSample();
    b.payload.key = "a";
    b.payload.value = "bc";
    EXPECT_NE(bytesToSign(a), bytesToSign(b));
}

TEST(BytesToSign, HasExpectedSize)
{
    const Message m = makeSample();
    const std::size_t expected =
        95 + m.payload.key.size() + m.payload.value.size() + m.result.value.size();
    EXPECT_EQ(bytesToSign(m).size(), expected);
}

// ---------- serialize ----------

TEST(Serialize, IsBytesToSignPlusSignature)
{
    const Message m = makeSample();
    const std::vector<unsigned char> signedPart = bytesToSign(m);
    const std::vector<unsigned char> all = serialize(m);

    ASSERT_EQ(all.size(), signedPart.size() + m.signature.size());

    const std::vector<unsigned char> start(all.begin(), all.begin() + signedPart.size());
    EXPECT_EQ(start, signedPart);

    const std::vector<unsigned char> end(all.end() - m.signature.size(), all.end());
    const std::vector<unsigned char> signature(m.signature.begin(), m.signature.end());
    EXPECT_EQ(end, signature);
}

// ---------- parse: valid input ----------

TEST(ParseValid, RoundTripOfSample)
{
    const Message original = makeSample();
    const auto parsed = parse(serialize(original));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, original);
}

TEST(ParseValid, RoundTripOfDefaultMessage)
{
    const Message original;
    const auto parsed = parse(serialize(original));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, original);
}

TEST(ParseValid, RoundTripOfEveryTypeOpAndStatus)
{
    const int lastType = static_cast<int>(MessageType::REPLY);
    const int lastStatus = static_cast<int>(Status::NotExecutedYet);

    for (int type = 0; type <= lastType; ++type)
    {
        for (const Operation op : {Operation::Put, Operation::Get})
        {
            for (int status = 0; status <= lastStatus; ++status)
            {
                Message original = makeSample();
                original.type = static_cast<MessageType>(type);
                original.payload.op = op;
                original.result.status = static_cast<Status>(status);

                const auto parsed = parse(serialize(original));
                ASSERT_TRUE(parsed.has_value()) << "type " << type << ", status " << status;
                EXPECT_EQ(*parsed, original);
            }
        }
    }
}

TEST(ParseValid, RoundTripOfKeyContainingZeroByte)
{
    Message original = makeSample();
    original.payload.key = std::string("a\0b", 3);
    const auto parsed = parse(serialize(original));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->payload.key.size(), 3u);
    EXPECT_EQ(*parsed, original);
}

TEST(ParseValid, AcceptsLastTypeValue)
{
    std::vector<unsigned char> bytes = serialize(makeSample());
    bytes[kTypeOffset] = static_cast<unsigned char>(MessageType::REPLY);
    EXPECT_TRUE(parse(bytes).has_value());
}

// ---------- parse: invalid input ----------

TEST(ParseInvalid, RejectsEmptyInput)
{
    EXPECT_FALSE(parse({}).has_value());
}

TEST(ParseInvalid, RejectsEveryTruncation)
{
    const std::vector<unsigned char> bytes = serialize(makeSample());
    for (std::size_t n = 0; n < bytes.size(); ++n)
    {
        const std::vector<unsigned char> truncated(bytes.begin(), bytes.begin() + n);
        EXPECT_FALSE(parse(truncated).has_value()) << "accepted the first " << n << " bytes";
    }
}

TEST(ParseInvalid, RejectsExtraByte)
{
    std::vector<unsigned char> bytes = serialize(makeSample());
    bytes.push_back(0);
    EXPECT_FALSE(parse(bytes).has_value());
}

TEST(ParseInvalid, RejectsUnknownType)
{
    for (const unsigned char badType : {5, 255})
    {
        std::vector<unsigned char> bytes = serialize(makeSample());
        bytes[kTypeOffset] = badType;
        EXPECT_FALSE(parse(bytes).has_value()) << "type byte " << int{badType};
    }
}

TEST(ParseInvalid, RejectsInvalidOp)
{
    std::vector<unsigned char> bytes = serialize(makeSample());
    bytes[kOpOffset] = 2;
    EXPECT_FALSE(parse(bytes).has_value());
}

TEST(ParseInvalid, RejectsInvalidStatus)
{
    const Message m = makeSample();
    std::vector<unsigned char> bytes = serialize(m);
    bytes[statusOffset(m)] = static_cast<unsigned char>(static_cast<int>(Status::NotExecutedYet) + 1);
    EXPECT_FALSE(parse(bytes).has_value());
}

TEST(ParseInvalid, RejectsHugeKeyLength)
{
    std::vector<unsigned char> bytes = serialize(makeSample());
    writeLength(bytes, kKeyLengthOffset, UINT64_MAX);
    EXPECT_FALSE(parse(bytes).has_value());
}

TEST(ParseInvalid, RejectsKeyLengthOneTooBig)
{
    const Message m = makeSample();
    std::vector<unsigned char> bytes = serialize(m);
    writeLength(bytes, kKeyLengthOffset, m.payload.key.size() + 1);
    EXPECT_FALSE(parse(bytes).has_value());
}