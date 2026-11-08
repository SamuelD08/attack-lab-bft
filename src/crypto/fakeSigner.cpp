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
    const std::uint32_t stored = ownerId + 1;
    std::memcpy(m.signature.data(), &stored, sizeof(stored));
}

bool FakeSigner::verify(const Message& m)
{
    std::uint32_t stored = 0;
    verifyCounter++;
    std::memcpy(&stored, m.signature.data(), sizeof(stored));
    return (stored == m.sender + 1);
}

std::uint64_t FakeSigner::getSignCounter() const
{
    return signCounter;
}

std::uint64_t FakeSigner::getVerifyCounter() const
{
    return verifyCounter;
}
