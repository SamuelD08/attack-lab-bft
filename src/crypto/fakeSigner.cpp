//
// Created by sam on 04/11/2026.
//

#include "fakeSigner.h"

#include <cstring>

FakeSigner::FakeSigner(const std::uint32_t id): ownerId(id){}

void FakeSigner::sign(Message& m)
{
    signCounter++;
    m.signature.fill(0);
    std::memcpy(m.signature.data(), &ownerId, sizeof(ownerId));
}

bool FakeSigner::verify(const Message& m)
{
    std::uint32_t id = 0;
    verifyCounter++;
    std::memcpy(&id, m.signature.data(), sizeof(id));
    return (id == m.sender);
}

std::uint64_t FakeSigner::getSignCounter() const
{
    return signCounter;
}

std::uint64_t FakeSigner::getVerifyCounter() const
{
    return verifyCounter;
}
