//
// Created by sam on 03/11/2026.
//

#include "kv_store.h"


Result KVStore::execute(const Request& req)
{
    Result res;
    if (req.key.empty())
    {
        res.found = Status::InvalidKey;
        return res;
    }
    switch (req.op)
    {
    case Operation::Put:
        data[req.key] = req.value;
        res.found = Status::Ok;
        res.value = req.value;
        break;
    case Operation::Get:
    {
        auto it = data.find(req.key);
        if (it != data.end())
        {
            res.value = it->second;
            res.found = Status::Ok;
        } else
        {
            res.found = Status::NotFound;
        }
        break;
    }
    }
    return res;
}

Digest KVStore::digest() const
{
    return {};
}
