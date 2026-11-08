//
// Created by sam on 07/11/2026.
//

#include "replica.h"

Replica::Replica(const std::uint32_t replicaId, Transport& network, const std::uint64_t maxFaulty, Signer& cryptoSigner): id(replicaId), transport(network), f(maxFaulty), n(3*f+1), signer(cryptoSigner)
{
}

void Replica::onMessage(const Message& m)
{
    // Check the request signature
    if (!signer.verify(m)) return;

    switch (m.type)
    {
    case MessageType::REQUEST:
        onMessageRequest(m);
        break;
    default:
        break;
    }

}

void Replica::onTimer(const std::uint64_t)
{
    // Nothing for the moment
}

void Replica::startRound(const Request& request)
{
    const std::uint64_t seq = nextSeq++;
    const Digest digest = requestDigest(request);

    // Fill RequestInformation
    RequestInformation& slot = requestInformation[seq];
    slot.digest = digest;
    slot.request = request;
    slot.preprepareAccepted = true;

    roundRunning = true;

    Message prePrepare;
    prePrepare.type = MessageType::PRE_PREPARE;
    prePrepare.sender = id;
    prePrepare.seq = seq;
    prePrepare.view = currentView;
    prePrepare.payload = request;
    prePrepare.digest = digest;

    for (std::uint32_t replica = 0; replica < (n); replica++)
    {
        if (replica != id)
        {
            prePrepare.receiver = replica;
            signer.sign(prePrepare);
            transport.send(prePrepare);
        }
    }
}

void Replica::onMessageRequest(const Message& message)
{
    const Request& request = message.payload;
    // Check if the clientId is consistent with senderId
    if (request.clientId != message.sender) return;

    // When a Request is receipt
    // Check that the replica is the primary
    if (getPrimary() != id)
    {
        return;
    }
    if (!isNewer(lastRequestId, request))
    {
        return;
    }
    lastRequestId[request.clientId] = request.requestId;

    // Check if there is no waiting request or we are not already in a round
    if (roundRunning || !waitingRequests.empty())
    {
        waitingRequests.push(request);
        return;
    }
    startRound(request);
}


std::uint32_t Replica::getPrimary() const
{
    return static_cast<std::uint32_t>(currentView % n);
}

const std::map<std::uint64_t, Request>& Replica::getLedger() const
{
    return ledger;
}

bool Replica::isNewer(const std::map<std::uint32_t, std::uint64_t>& last, const Request& request)
{
    const auto it = last.find(request.clientId); // find, not []: reading must not create entries
    const std::uint64_t lastId = (it == last.end()) ? 0 : it->second;
    return request.requestId > lastId;
}
