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
    case MessageType::PRE_PREPARE:
        onMessagePrePrepare(m);
        break;
    case MessageType::PREPARE:
        onMessagePrepare(m);
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

void Replica::sendCommitIfPrepared(const std::uint64_t seq)
{
    auto it = requestInformation.find(seq);
    if (it == requestInformation.end()) return;
    RequestInformation& slot = it->second;
    if (!slot.preprepareAccepted || slot.commitSent) return;
    const auto voters = slot.whoVotePrepare.find(slot.digest);
    if (voters == slot.whoVotePrepare.end()) return;
    if (const std::size_t numVotes = voters->second.size(); numVotes < 2*f) return;

    // Create commit message
    slot.commitSent = true;
    Message commit;
    commit.digest = slot.digest;
    commit.sender = id;
    commit.seq = seq;
    commit.view = currentView;
    commit.type = MessageType::COMMIT;

    // Record my own commit
    slot.whoVoteCommit[slot.digest].insert(id);

    //Send it
    for (std::uint32_t replica = 0; replica < n; replica++)
    {
        if (replica == id) continue;
        commit.receiver = replica;
        signer.sign(commit);
        transport.send(commit);
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

void Replica::onMessagePrePrepare(const Message& m)
{
    // Check if it is from the primary
    if (getPrimary() != m.sender) return;

    // Check the view
    if (currentView != m.view) return;

    // Check the digest
    if (m.digest != requestDigest(m.payload)) return;

    // Check that no other request was already accepted for this seq
    auto it = requestInformation.find(m.seq);
    if (it != requestInformation.end() && it->second.preprepareAccepted) return;

    // Record the slot
    RequestInformation& slot = requestInformation[m.seq];
    slot.request = m.payload;
    slot.digest = m.digest;
    slot.preprepareAccepted = true;
    // Record my own vote
    slot.whoVotePrepare[m.digest].insert(id);

    // Create a PREPARE message
    Message prepare;
    prepare.type = MessageType::PREPARE;
    prepare.sender = id;
    prepare.digest = slot.digest;
    prepare.view = currentView;
    prepare.seq = m.seq;

    // Send this message
    for (std::uint32_t replica = 0; replica < n; replica++)
    {
        if (replica == id) continue;
        prepare.receiver = replica;
        signer.sign(prepare);
        transport.send(prepare);
    }
    sendCommitIfPrepared(prepare.seq);
}

void Replica::onMessagePrepare(const Message& m)
{
    // Check that the sender is a replica and not a client
    if (m.sender >= n) return;

    // Check if it is not from the primary (the primary never send PREPARE)
    if (m.sender == getPrimary()) return;

    // Check the view
    if (currentView != m.view) return;

    // Record the sender in the votes
    RequestInformation& slot = requestInformation[m.seq];
    slot.whoVotePrepare[m.digest].insert(m.sender);

    sendCommitIfPrepared(m.seq);
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
