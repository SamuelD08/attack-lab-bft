//
// Created by sam on 21/11/2026.
//

#include "client.h"

#include <cassert>

Client::Client(const std::uint32_t clientId, Transport& network, Signer& cryptoSigner, const std::uint64_t maxFaulty): id(clientId), transport(network), signer(cryptoSigner), f(maxFaulty), n(3*f+1)
{
    assert(clientId >= n);
}

bool Client::sendRequest(Request r)
{
    if (pending) return false; // a request is waiting -> refuse

    // assign all parameters to the request
    r.requestId = nextRequestId++;
    r.clientId = id;

    pending = r;
    pendingSentAt = transport.now();

    sendToReplicas(r);

    // Set the timer
    transport.setTimer(id, requestTimeout, r.requestId);
    return true;
}


void Client::sendToReplicas(const Request& r) const
{
    // Create the message
    // we don't care about currentView, seq as a client
    Message m;
    m.payload = r;
    m.sender = id;
    m.type = MessageType::REQUEST;
    m.digest = requestDigest(r);

    for (std::uint32_t i = 0; i < n; i++)
    {
        m.receiver = i;
        signer.sign(m);
        transport.send(m);
    }
}

void Client::onTimer(std::uint64_t timerId)
{
    // The request is already complete
    if (!pending || pending->requestId != timerId) return;

    // Still waiting -> we have to resend the same request (with the same request ID) and wait again
    if (numRetry >= maxRetry)
    {
        // TODO: record the failure
        allClear();
        return;
    }
    sendToReplicas(*pending);
    transport.setTimer(id, requestTimeout, timerId);
    numRetry++;
}

void Client::onMessage(const Message& m)
{
    // Verify
    if (!signer.verify(m)) return;

    //Check
    if (m.type != MessageType::REPLY) return;
    if (m.sender >= n) return;
    if (!pending) return;
    if (m.payload.requestId != pending->requestId) return;

    // Remember
    replies.insert({m.sender, m.result});

    // Act
    if (enoughMatchingResult(m.result))
    {
        results.insert({m.payload.requestId, m.result});
        complete();
    }
}

bool Client::enoughMatchingResult(const Result& res) const
{
    std::uint64_t numMatchingResult = 0;
    for (const auto& [replicaId, result] : replies)
    {
        if (result == res) numMatchingResult++;
    }
    return numMatchingResult >= f+1;
}

void Client::complete()
{
    latencies.push_back(transport.now() - pendingSentAt);
    allClear();
}


bool Client::isWaiting() const
{
    return pending.has_value();
}

const std::vector<std::uint64_t>& Client::getLatencies() const
{
    return latencies;
}

const std::map<std::uint64_t, Result>& Client::getResults() const
{
    return results;
}

void Client::allClear()
{
    pending.reset();
    pendingSentAt = 0;
    numRetry = 0;
    replies.clear();
}

