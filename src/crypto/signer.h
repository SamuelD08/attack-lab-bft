//
// Created by sam on 04/11/2026.
//

#ifndef ATTACK_LAB_BFT_SIGNER_H
#define ATTACK_LAB_BFT_SIGNER_H
#include "protocol/message.h"

class Signer
{
public:
    virtual void sign(Message& m) = 0;
    virtual bool verify(const Message& m) = 0;

    virtual ~Signer() = default;
};

#endif //ATTACK_LAB_BFT_SIGNER_H
