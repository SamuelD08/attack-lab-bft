//
// Created by sam on 22/11/2026.
//

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "crypto/fakeSigner.h"
#include "protocol/replica.h"
#include "protocol/client.h"
#include "sim/deliveryPolicy.h"
#include "sim/simulator.h"
#include "test_node.h"

namespace
{
    constexpr std::uint32_t kClientId = 100;
    constexpr std::uint64_t kMaxFaulty = 1;  // f = 1, so n = 4 replicas and f + 1 = 2 matching replies
    constexpr std::uint64_t kMaxRetry = 5;   // same value as maxRetry in Client

    // ------ HELPERS ----------
    // A fake network. it delivers nothing, it only writes down what the client does.
    class RecordingTransport : public Transport
    {
    public:
        struct Timer
        {
            std::uint32_t nodeId;
            std::uint64_t delay;
            std::uint64_t timerId;
        };

        std::vector<Message> sent;
        std::vector<Timer> timers;
        std::uint64_t time = 0;

        void send(const Message& m) override { sent.push_back(m); }
        void setTimer(std::uint32_t nodeId, std::uint64_t delay, std::uint64_t timerId) override
        {
            timers.push_back({nodeId, delay, timerId});
        }
        [[nodiscard]] std::uint64_t now() const override { return time; }


    };

    Request putRequest(const std::string& key, const std::string& value)
    {
        Request r;
        r.op = Operation::Put;
        r.key = key;
        r.value = value;
        return r;
    }

    Result okResult(const std::string& value)
    {
        Result res;
        res.status = Status::Ok;
        res.value = value;
        return res;
    }

    // A REPLY from replica 'from' signed by that replica.
    Message reply(std::uint32_t from, std::uint64_t requestId, const Result& result)
    {
        Message m;
        m.type = MessageType::REPLY;
        m.sender = from;
        m.receiver = kClientId;
        m.payload.clientId = kClientId;
        m.payload.requestId = requestId;
        m.result = result;
        FakeSigner signer(from);
        signer.sign(m);
        return m;
    }

    // The client and its fake network, ready to use. To avoid to repeat the same code each test
    struct ClientFixture : public ::testing::Test
    {
        RecordingTransport network;
        FakeSigner signer{kClientId};
        Client client{kClientId, network, signer, kMaxFaulty};
    };

    // --------------------------------------------- sending

    TEST_F(ClientFixture, SendsTheRequestToEveryReplica)
    {
        ASSERT_TRUE(client.sendRequest(putRequest("x", "1")));

        ASSERT_EQ(network.sent.size(), 4);
        for (std::uint32_t i = 0; i < 4; i++)
        {
            const Message& m = network.sent[i];
            EXPECT_EQ(m.type, MessageType::REQUEST);
            EXPECT_EQ(m.sender, kClientId);
            EXPECT_EQ(m.receiver, i);
            EXPECT_EQ(m.payload.clientId, kClientId);
            EXPECT_EQ(m.payload.requestId, 1);       // request ids start at 1
            EXPECT_EQ(m.payload.key, "x");
            EXPECT_TRUE(signer.verify(m));            // signed by the client
        }
        EXPECT_TRUE(client.isWaiting());
    }

    TEST_F(ClientFixture, SetsOneTimerPerRequestNamedAfterTheRequest)
    {
        client.sendRequest(putRequest("x", "1"));

        ASSERT_EQ(network.timers.size(), 1);
        EXPECT_EQ(network.timers[0].nodeId, kClientId);
        EXPECT_EQ(network.timers[0].timerId, 1);
    }


    TEST_F(ClientFixture, RefusesASecondRequestWhileWaiting)
    {
        EXPECT_TRUE(client.sendRequest(putRequest("x", "1")));
        EXPECT_FALSE(client.sendRequest(putRequest("y", "2")));

        EXPECT_EQ(network.sent.size(), 4);
    }

    TEST_F(ClientFixture, NextRequestGetsTheNextId)
    {
        client.sendRequest(putRequest("x", "1"));
        client.onMessage(reply(0, 1, okResult("")));
        client.onMessage(reply(1, 1, okResult("")));
        ASSERT_FALSE(client.isWaiting());

        ASSERT_TRUE(client.sendRequest(putRequest("y", "2")));
        EXPECT_EQ(network.sent.back().payload.requestId, 2);
    }

    // ----------------------------- collecting replies

    TEST_F(ClientFixture, CompletesAfterFPlusOneMatchingReplies)
    {
        client.sendRequest(putRequest("x", "1"));

        client.onMessage(reply(0, 1, okResult("done")));
        client.onMessage(reply(1, 1, okResult("done")));

        EXPECT_FALSE(client.isWaiting());
        ASSERT_EQ(client.getResults().count(1), 1);
        EXPECT_EQ(client.getResults().at(1), okResult("done"));
        EXPECT_EQ(client.getLatencies().size(), 1);
    }

    TEST_F(ClientFixture, OneReplyIsNotEnough)
    {
        client.sendRequest(putRequest("x", "1"));

        client.onMessage(reply(0, 1, okResult("done")));

        EXPECT_TRUE(client.isWaiting());
        EXPECT_TRUE(client.getResults().empty());
    }

    TEST_F(ClientFixture, DifferentResultsDoNotMatch)
    {
        client.sendRequest(putRequest("x", "1"));

        client.onMessage(reply(0, 1, okResult("A")));
        client.onMessage(reply(3, 1, okResult("B"))); // a lying replica
        EXPECT_TRUE(client.isWaiting());

        client.onMessage(reply(1, 1, okResult("A")));
        EXPECT_FALSE(client.isWaiting());
        EXPECT_EQ(client.getResults().at(1), okResult("A")); // the majority answer and not the lie
    }

    TEST_F(ClientFixture, TheSameReplicaCountsOnlyOnce)
    {
        client.sendRequest(putRequest("x", "1"));

        client.onMessage(reply(3, 1, okResult("fake")));
        client.onMessage(reply(3, 1, okResult("fake")));

        EXPECT_TRUE(client.isWaiting());
    }


    TEST_F(ClientFixture, AReplicaCannotChangeItsAnswer)
    {
        client.sendRequest(putRequest("x", "1"));

        client.onMessage(reply(3, 1, okResult("fake")));
        client.onMessage(reply(3, 1, okResult("real"))); // ignored: replica 3 already answered
        client.onMessage(reply(0, 1, okResult("real")));

        EXPECT_TRUE(client.isWaiting()); // only replica 0 says "real"
    }

    // ----------- messages that must be ignored

    TEST_F(ClientFixture, IgnoresAnUnsignedReply)
    {
        client.sendRequest(putRequest("x", "1"));

        Message unsigned_ = reply(1, 1, okResult("done"));
        unsigned_.signature.fill(0);
        client.onMessage(reply(0, 1, okResult("done")));
        client.onMessage(unsigned_);

        EXPECT_TRUE(client.isWaiting());
    }

    TEST_F(ClientFixture, IgnoresAReplyFromANodeThatIsNotAReplica)
    {
        client.sendRequest(putRequest("x", "1"));

        client.onMessage(reply(0, 1, okResult("done")));
        client.onMessage(reply(7, 1, okResult("done"))); // 7 >= n: not a replica

        EXPECT_TRUE(client.isWaiting());
    }

    TEST_F(ClientFixture, IgnoresAReplyForAnotherRequest)
    {
        client.sendRequest(putRequest("x", "1"));

        client.onMessage(reply(0, 1, okResult("done")));
        client.onMessage(reply(1, 99, okResult("done")));

        EXPECT_TRUE(client.isWaiting());
    }

    TEST_F(ClientFixture, IgnoresMessagesThatAreNotReplies)
    {
        client.sendRequest(putRequest("x", "1"));

        Message commit0 = reply(0, 1, okResult("done"));
        Message commit1 = reply(1, 1, okResult("done"));
        commit0.type = MessageType::COMMIT;
        commit1.type = MessageType::COMMIT;
        FakeSigner(0).sign(commit0); // sign again because the type is part of the signature
        FakeSigner(1).sign(commit1);
        client.onMessage(commit0);
        client.onMessage(commit1);

        EXPECT_TRUE(client.isWaiting());
    }

    TEST_F(ClientFixture, IgnoresRepliesWhenNothingIsPending)
    {
        client.onMessage(reply(0, 1, okResult("done")));
        client.onMessage(reply(1, 1, okResult("done")));

        EXPECT_TRUE(client.getResults().empty());
        EXPECT_TRUE(client.getLatencies().empty());
    }

    TEST_F(ClientFixture, LateRepliesAfterCompletionChangeNothing)
    {
        client.sendRequest(putRequest("x", "1"));
        client.onMessage(reply(0, 1, okResult("done")));
        client.onMessage(reply(1, 1, okResult("done")));

        client.onMessage(reply(2, 1, okResult("done")));
        client.onMessage(reply(3, 1, okResult("done")));

        EXPECT_EQ(client.getResults().size(), 1);
        EXPECT_EQ(client.getLatencies().size(), 1);
    }

    // ----------------------- latency

    TEST_F(ClientFixture, LatencyIsTheTimeFromSendToCompletion)
    {
        network.time = 50;
        client.sendRequest(putRequest("x", "1"));

        network.time = 470;
        client.onMessage(reply(0, 1, okResult("done")));
        client.onMessage(reply(1, 1, okResult("done")));

        ASSERT_EQ(client.getLatencies().size(), 1);
        EXPECT_EQ(client.getLatencies()[0], 420);
    }

    TEST_F(ClientFixture, LatencyCountsFromTheFirstSendEvenAfterARetry)
    {
        network.time = 0;
        client.sendRequest(putRequest("x", "1"));
        network.time = 1000;
        client.onTimer(1); // retry

        network.time = 1500;
        client.onMessage(reply(0, 1, okResult("done")));
        client.onMessage(reply(1, 1, okResult("done")));

        ASSERT_EQ(client.getLatencies().size(), 1);
        EXPECT_EQ(client.getLatencies()[0], 1500);
    }

    // --------------------------- timers and retries

    TEST_F(ClientFixture, TimeoutResendsTheSameRequest)
    {
        client.sendRequest(putRequest("x", "1"));

        client.onTimer(1);

        ASSERT_EQ(network.sent.size(), 8); // 4 the first time, 4 for the retry
        for (std::size_t i = 4; i < 8; i++)
        {
            EXPECT_EQ(network.sent[i].type, MessageType::REQUEST);
            EXPECT_EQ(network.sent[i].payload.requestId, 1); // same id so not a new request
        }
        ASSERT_EQ(network.timers.size(), 2); // a new timer for the retry
        EXPECT_EQ(network.timers[1].timerId, 1);
        EXPECT_TRUE(client.isWaiting());
    }

    TEST_F(ClientFixture, RepliesFromBeforeARetryStillCount)
    {
        client.sendRequest(putRequest("x", "1"));
        client.onMessage(reply(0, 1, okResult("done")));

        client.onTimer(1);
        client.onMessage(reply(1, 1, okResult("done")));

        EXPECT_FALSE(client.isWaiting());
    }

    TEST_F(ClientFixture, AnOldTimerIsIgnored)
    {
        client.sendRequest(putRequest("x", "1"));
        client.onMessage(reply(0, 1, okResult("done")));
        client.onMessage(reply(1, 1, okResult("done")));

        client.onTimer(1); // fires after the request was answered

        EXPECT_EQ(network.sent.size(), 4); // no resend
    }

    TEST_F(ClientFixture, GivesUpAfterTheMaximumNumberOfRetries)
    {
        client.sendRequest(putRequest("x", "1"));

        for (std::uint64_t i = 0; i < kMaxRetry; i++)
        {
            client.onTimer(1);
            EXPECT_TRUE(client.isWaiting()); // still trying
        }
        client.onTimer(1);

        EXPECT_FALSE(client.isWaiting());
        EXPECT_EQ(network.sent.size(), 4 * (1 + kMaxRetry)); // first send + 5 retries
        EXPECT_TRUE(client.getResults().empty());   // a failed request has no result
        EXPECT_TRUE(client.getLatencies().empty());
    }


    // ===================================================== Part 2: the real system =============================================

    // Drops every message sent by one node
    class SilentNode : public DeliveryPolicy
    {
    private:
        std::uint32_t silent;
    public:
        explicit SilentNode(std::uint32_t nodeId) : silent(nodeId) {}
        std::vector<std::uint64_t> decide(const Message& message, std::uint64_t) override
        {
            if (message.sender == silent) return {};
            return {0};
        }

    };

    // Four real replicas (f = 1) and one real client.
    struct System
    {
        Simulator sim;
        FakeSigner s0{0}, s1{1}, s2{2}, s3{3}, sc{kClientId};
        Replica r0{0, sim, kMaxFaulty, s0};
        Replica r1{1, sim, kMaxFaulty, s1};
        Replica r2{2, sim, kMaxFaulty, s2};
        Replica r3{3, sim, kMaxFaulty, s3};
        Client client{kClientId, sim, sc, kMaxFaulty};

        System(std::uint64_t seed, std::uint64_t maxExtraDelay) : sim(seed, 100, maxExtraDelay)
        {
            sim.addNode(0, r0);
            sim.addNode(1, r1);
            sim.addNode(2, r2);
            sim.addNode(3, r3);
            sim.addNode(kClientId, client);
        }
    };

    TEST(ClientSystem, OneRequestCompletes)
    {
        System s(1, 0);

        ASSERT_TRUE(s.client.sendRequest(putRequest("x", "hello")));
        s.sim.run(kForever);

        EXPECT_FALSE(s.client.isWaiting());
        EXPECT_EQ(s.client.getResults().count(1), 1);
        ASSERT_EQ(s.client.getLatencies().size(), 1);
        EXPECT_GT(s.client.getLatencies()[0], 0);
    }

    TEST(ClientSystem, AGetReturnsWhatAPutWrote)
    {
        System s(1, 0);

        s.client.sendRequest(putRequest("x", "hello"));
        s.sim.run(kForever);

        Request get;
        get.op = Operation::Get;
        get.key = "x";
        ASSERT_TRUE(s.client.sendRequest(get));
        s.sim.run(kForever);

        ASSERT_EQ(s.client.getResults().count(2), 1);
        EXPECT_EQ(s.client.getResults().at(2).value, "hello");
    }

    TEST(ClientSystem, FiftyRequestsOneAtATimeWithRandomDelays)
    {
        System s(42, 50);

        for (int i = 0; i < 50; i++)
        {
            ASSERT_TRUE(s.client.sendRequest(putRequest("key", std::to_string(i)))) << "request " << i;
            s.sim.run(kForever);
            ASSERT_FALSE(s.client.isWaiting()) << "request " << i << " did not complete";
        }

        EXPECT_EQ(s.client.getResults().size(), 50);
        EXPECT_EQ(s.client.getLatencies().size(), 50);
        EXPECT_EQ(s.r0.getLedger().size(), 50);
        EXPECT_EQ(s.r3.getLedger().size(), 50);
    }

    TEST(ClientSystem, ASilentBackupDoesNotStopTheClient)
    {
        System s(1, 0);
        SilentNode silentBackup(3);
        s.sim.setNetworkPolicy(silentBackup);

        s.client.sendRequest(putRequest("x", "1"));
        s.sim.run(kForever);

        EXPECT_FALSE(s.client.isWaiting());
        EXPECT_EQ(s.client.getResults().count(1), 1);
    }

    TEST(ClientSystem, WithASilentPrimaryTheClientGivesUp)
    {
        System s(1, 0);
        SilentNode silentPrimary(0);
        s.sim.setNetworkPolicy(silentPrimary);

        s.client.sendRequest(putRequest("x", "1"));
        s.sim.run(kForever); // the client gives up after 5 retries

        EXPECT_FALSE(s.client.isWaiting());
        EXPECT_TRUE(s.client.getResults().empty());
        EXPECT_TRUE(s.client.getLatencies().empty());
    }
}