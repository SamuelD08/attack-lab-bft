//
// Created by sam on 03/11/2026.
//

#ifndef ATTACK_LAB_BFT_KV_STORE_H
#define ATTACK_LAB_BFT_KV_STORE_H
#include <cstdint>
#include <map>
#include <string>
#include <sodium.h>
#include <array>

enum class Operation
{
    Put,
    Get
};

enum class Status
{
    Ok,
    NotFound,
    InvalidKey,
    NotExecutedYet
};

struct Request
{
    std::uint32_t clientId = 0;
    std::uint64_t requestId = 0;
    Operation op = Operation::Get;
    std::string key;
    std::string value;

    bool operator==(const Request&) const = default;
};

struct Result
{
    Status found = Status::NotExecutedYet;
    std::string value;

    bool operator==(const Result&) const = default;
};

using Digest = std::array<unsigned char, crypto_hash_sha256_BYTES>;

class KVStore
{
private:
    std::map<std::string, std::string> data;

public:
    Result execute(const Request& req);

    [[nodiscard]] Digest digest() const;
};


#endif //ATTACK_LAB_BFT_KV_STORE_H
