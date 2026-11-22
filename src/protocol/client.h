//
// Created by sam on 21/11/2026.
//

#ifndef ATTACK_LAB_BFT_CLIENT_H
#define ATTACK_LAB_BFT_CLIENT_H
#include "crypto/signer.h"
#include "sim/node.h"
#include "sim/transport.h"
#include "message.h"
#include "kv/kv_store.h" // for request

#include <cstdint>
#include <optional>
#include <vector>
#include <map>


class Client: public Node
{
private:
    std::uint32_t id = 1;

    const std::uint64_t requestTimeout = 1000;
    const std::uint64_t maxRetry = 5;
    std::uint64_t numRetry = 0;
    Transport& transport;
    Signer& signer;
    std::uint64_t nextRequestId = 1;

    std::uint64_t f = 0; // max number of failed replica
    const std::uint64_t n;

    std::optional<Request> pending; // The request we're waiting for. If it is empty -> there is no request
    std::uint64_t pendingSentAt = 0; // when it was first sent
    std::vector<std::uint64_t> latencies; // one entry per complete request -> it is for the measurement (Later)

    std::map<std::uint32_t, Result> replies;
    std::map<std::uint64_t, Result> results;

    void sendToReplicas(const Request& r) const;
    // That completes a request
    void complete();
    [[nodiscard]] bool enoughMatchingResult(const Result& res) const;
    void allClear();

public:
    Client(std::uint32_t clientId, Transport &network, Signer& cryptoSigner, std::uint64_t maxFaulty);

    // Client collects the REPLYs here.
    void onMessage(const Message& m) override;
    void onTimer(std::uint64_t timerId) override;

    bool sendRequest(Request r);

    [[nodiscard]] bool isWaiting() const;
    [[nodiscard]] const std::vector<std::uint64_t>& getLatencies() const;
    [[nodiscard]] const std::map<std::uint64_t, Result>& getResults() const;
};


#endif //ATTACK_LAB_BFT_CLIENT_H
