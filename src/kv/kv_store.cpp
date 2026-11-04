//
// Created by sam on 03/11/2026.
//

#include "kv_store.h"


Result KVStore::execute(const Request& req)
{
    Result res;
    if (req.key.empty())
    {
        res.status = Status::InvalidKey;
        return res;
    }
    switch (req.op)
    {
    case Operation::Put:
        data[req.key] = req.value;
        res.status = Status::Ok;
        res.value = req.value;
        break;
    case Operation::Get:
    {
        auto it = data.find(req.key);
        if (it != data.end())
        {
            res.value = it->second;
            res.status = Status::Ok;
        } else
        {
            res.status = Status::NotFound;
        }
        break;
    }
    }
    return res;
}

Digest KVStore::digest() const
{
    Digest d;
    crypto_hash_sha256_state state;
    crypto_hash_sha256_init(&state);
    for (const auto& [key, value] : data)
    {
        // For each (key, value) in key order: len(key) as 8 bytes | key | len(value) as 8 bytes | value
        const std::uint64_t kLength = key.length();
        const std::uint64_t vLength = value.length();
        crypto_hash_sha256_update(&state, reinterpret_cast<const unsigned char*>(&kLength), sizeof(kLength));
        crypto_hash_sha256_update(&state, reinterpret_cast<const unsigned char*>(key.data()), kLength);
        crypto_hash_sha256_update(&state, reinterpret_cast<const unsigned char*>(&vLength), sizeof(vLength));
        crypto_hash_sha256_update(&state, reinterpret_cast<const unsigned char*>(value.data()), vLength);
    }
    crypto_hash_sha256_final(&state, d.data());
    return d;
}
