//
// Created by sam on 04/11/2026.
//

#ifndef ATTACK_LAB_BFT_FAKESIGNER_H
#define ATTACK_LAB_BFT_FAKESIGNER_H
#include "signer.h"
#include "protocol/message.h"


class FakeSigner: public Signer
{
private:
    std::uint32_t ownerId;
    std::uint64_t signCounter = 0;
    std::uint64_t verifyCounter = 0;
public:
    explicit FakeSigner(std::uint32_t id);

    void sign(Message& m) override;
    bool verify(const Message& m) override;

    [[nodiscard]] std::uint64_t getSignCounter() const;
    [[nodiscard]] std::uint64_t getVerifyCounter() const;
};


#endif //ATTACK_LAB_BFT_FAKESIGNER_H
