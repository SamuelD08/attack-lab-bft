//
// Created by sam on 07/11/2026.
//

#ifndef ATTACK_LAB_BFT_REPLICA_H
#define ATTACK_LAB_BFT_REPLICA_H
#include <queue>
#include <set>
#include <map>

#include "sim/node.h"
#include "sim/transport.h"
#include "crypto/signer.h"
#include "kv/kv_store.h"
#include "protocol/message.h"

struct RequestInformation
{
    Request request;
    Digest digest{};
    std::map<Digest, std::set<std::uint32_t>> whoVotePrepare; // contain replica Id
    std::map<Digest, std::set<std::uint32_t>> whoVoteCommit; // contain replica Id
    bool preprepareAccepted = false;
    bool commitSent = false;
    bool isExecuted = false;
};

class Replica: public Node
{
private:
    std::uint32_t id = 0;
    Transport& transport;
    std::uint64_t f = 0; // max number of failed replica
    const std::uint64_t n;
    Signer& signer;
    std::uint64_t expectedSeq = 1; // Not for primary, to check if the seq of the preprepare request is consistent
    std::uint64_t currentView = 0; // To compute the primary (the primary is currentView % n)
    std::uint64_t nextSeq = 1; // Primary only, to number the request; the first request has seq 1
    std::queue<Request> waitingRequests; // Primary only, the queue of waiting request -> one request at a time
    std::map<std::uint64_t, RequestInformation> requestInformation; // to remember actions for each slot (keyed by seq)
    KVStore kv; // state machine
    std::map<std::uint64_t, Request> ledger; // every operation in the order
    std::map<std::uint32_t, std::uint64_t>  lastRequestId; // To avoid the same request twice (duplicate), one per client, first -> clientID, second -> lastRequestSeq
    bool roundRunning = false;

    [[nodiscard]] std::uint32_t getPrimary() const;

    void startRound(const Request& request);
    void sendCommitIfPrepared(std::uint64_t seq);
    void onMessageRequest(const Message& message);
    void onMessagePrePrepare(const Message& m);
    void onMessagePrepare(const Message& m);

    // To check if the requestId from a client is an older one or not
    static bool isNewer(const std::map<std::uint32_t, std::uint64_t>& last, const Request& request);


public:
    Replica(std::uint32_t replicaId, Transport& network, std::uint64_t maxFaulty, Signer& cryptoSigner);

    void onMessage(const Message& m) override;
    void onTimer(std::uint64_t timerId) override;

    [[nodiscard]] const std::map<std::uint64_t, Request>& getLedger() const;
};

#endif //ATTACK_LAB_BFT_REPLICA_H
