//
// Created by sam on 04/11/2026.
//
// Tests for the Signer interface and the FakeSigner.
#include <gtest/gtest.h>

#include "crypto/fakeSigner.h"
#include "protocol/message.h"

namespace
{
    // A message that claims to come from the given node.
    Message messageFrom(std::uint32_t sender)
    {
        Message m;
        m.sender = sender;
        m.receiver = 1;
        m.type = MessageType::PREPARE;
        m.seq = 5;
        return m;
    }
}

// ---------- sign and verify ----------

TEST(FakeSigner, AcceptsMessageSignedByItsSender)
{
    FakeSigner replica0(0);
    FakeSigner replica1(1);

    Message m = messageFrom(0);
    replica0.sign(m);

    EXPECT_TRUE(replica1.verify(m));
}

TEST(FakeSigner, AcceptsMessageFromAnyNode)
{
    FakeSigner replica3(3);
    FakeSigner replica1(1);

    Message m = messageFrom(3);
    replica3.sign(m);

    EXPECT_TRUE(replica1.verify(m));
}

TEST(FakeSigner, RejectsForgedSender)
{
    FakeSigner replica0(0);
    FakeSigner replica1(1);

    Message m = messageFrom(0);
    replica0.sign(m);
    m.sender = 2;   // someone changes the sender after signing

    EXPECT_FALSE(replica1.verify(m));
}

TEST(FakeSigner, RejectsMessageSignedByAnotherNode)
{
    FakeSigner evilReplica2(2);
    FakeSigner replica1(1);

    Message m = messageFrom(0);   // claims to come from replica 0
    evilReplica2.sign(m);         // but replica 2 signs it

    EXPECT_FALSE(replica1.verify(m));
}

TEST(FakeSigner, RejectsUnsignedMessage)
{
    FakeSigner replica1(1);
    const Message m = messageFrom(5);   // never signed: the signature is all zeros

    EXPECT_FALSE(replica1.verify(m));
}

TEST(FakeSigner, SigningReplacesAnOldSignature)
{
    FakeSigner replica2(2);
    FakeSigner replica0(0);
    FakeSigner replica1(1);

    Message m = messageFrom(0);
    replica2.sign(m);   // first signature: replica 2
    replica0.sign(m);   // signed again: replica 0

    EXPECT_TRUE(replica1.verify(m));

    // Only the first 4 bytes hold the id; the rest must be cleared.
    for (std::size_t i = sizeof(std::uint32_t); i < m.signature.size(); ++i)
    {
        EXPECT_EQ(m.signature[i], 0) << "byte " << i << " was not cleared";
    }
}

TEST(FakeSigner, VerifyDoesNotChangeTheMessage)
{
    FakeSigner replica0(0);
    FakeSigner replica1(1);

    Message m = messageFrom(0);
    replica0.sign(m);
    const Message before = m;
    replica1.verify(m);

    EXPECT_EQ(m, before);
}

// ---------- counters ----------

TEST(FakeSignerCounters, StartAtZero)
{
    const FakeSigner signer(0);
    EXPECT_EQ(signer.getSignCounter(), 0);
    EXPECT_EQ(signer.getVerifyCounter(), 0);
}

TEST(FakeSignerCounters, CountEachNodeSeparately)
{
    FakeSigner replica0(0);
    FakeSigner replica1(1);

    Message m = messageFrom(0);
    replica0.sign(m);
    replica1.verify(m);

    EXPECT_EQ(replica0.getSignCounter(), 1);
    EXPECT_EQ(replica0.getVerifyCounter(), 0);
    EXPECT_EQ(replica1.getSignCounter(), 0);
    EXPECT_EQ(replica1.getVerifyCounter(), 1);
}

TEST(FakeSignerCounters, CountFailedVerificationsToo)
{
    FakeSigner replica0(0);
    FakeSigner replica1(1);

    Message good = messageFrom(0);
    replica0.sign(good);
    Message forged = good;
    forged.sender = 2;

    replica1.verify(good);     // succeeds
    replica1.verify(forged);   // fails, but the work was still done
    replica1.verify(good);     // succeeds

    EXPECT_EQ(replica1.getVerifyCounter(), 3);
}

// ---------- through the interface (as the protocol will use it) ----------

TEST(SignerInterface, WorksThroughBaseClassReference)
{
    FakeSigner replica0(0);
    FakeSigner replica1(1);
    Signer& signer0 = replica0;
    Signer& signer1 = replica1;

    Message m = messageFrom(0);
    signer0.sign(m);

    EXPECT_TRUE(signer1.verify(m));
    EXPECT_EQ(replica0.getSignCounter(), 1);
    EXPECT_EQ(replica1.getVerifyCounter(), 1);
}
