

#include <iostream>
#include <map>
#include <string>
#include <limits>

#include "crypto/fakeSigner.h"
#include "protocol/replica.h"
#include "sim/node.h"
#include "sim/simulator.h"
#include "kv/kv_store.h"
#include "protocol/message.h"
#include "sim/deliveryPolicy.h"

namespace
{
    constexpr std::uint64_t kForever = std::numeric_limits<std::uint64_t>::max();

    // A node that writes down everything it receives, then (optionally) reacts.
    class ClientNode : public Node
    {
    private:
        std::uint32_t id;
        const Transport& transport;

    public:
        ClientNode(std::uint32_t nodeId, const Transport& network)
            : id(nodeId), transport(network)
        {
        }

        void onMessage(const Message& m) override
        {

        }
        void onTimer(std::uint64_t timerId) override
        {

        }
    };

    Message clientRequest(std::uint32_t client, std::uint64_t requestId)
    {
        Message m;
        m.type = MessageType::REQUEST;
        m.sender = client;
        m.receiver = 0; // the primary in view 0
        m.payload.clientId = client;
        m.payload.requestId = requestId;
        m.payload.op = Operation::Put;
        m.payload.key = "key" + std::to_string(client);
        m.payload.value = std::to_string(requestId);
        FakeSigner signer(client);
        signer.sign(m);
        return m;
    }

    struct System
    {
        Simulator sim;
        FakeSigner s0{0}, s1{1}, s2{2}, s3{3};
        Replica r0{0, sim, 1, s0};
        Replica r1{1, sim, 1, s1};
        Replica r2{2, sim, 1, s2};
        Replica r3{3, sim, 1, s3};
        ClientNode c100{100, sim};

        System(std::uint64_t seed, std::uint64_t maxExtraDelay) : sim(seed, 100, maxExtraDelay)
        {
            sim.addNode(0, r0);
            sim.addNode(1, r1);
            sim.addNode(2, r2);
            sim.addNode(3, r3);
            sim.addNode(100, c100);
        }
    };

    class SilentPolicy : public DeliveryPolicy
    {
    private:
        std::uint32_t id;

    public:
        explicit SilentPolicy(const std::uint32_t silentId): id(silentId)
        {
        }

        std::vector<std::uint64_t> decide(const Message& message, std::uint64_t now) override
        {
            if (message.sender == id) return {};
            return {0};
        }
    };
}

static int normal(const std::uint64_t seed)
{
    // Create the System
    System system(seed, 0);

    // Create the request and send it
    system.sim.send(clientRequest(100, 1));
    system.sim.run(kForever);

    system.sim.printTrace(std::cout);

    return 0;
}

static int silent_backup(const std::uint64_t seed)
{
    // Create the System
    System system(seed, 0);

    // set a Network Policy
    auto policy = SilentPolicy(3);
    system.sim.setNetworkPolicy(policy);

    // Create the request and send it
    system.sim.send(clientRequest(100, 1));
    system.sim.run(kForever);

    system.sim.printTrace(std::cout);

    return 0;
}

static int silent_primary(const std::uint64_t seed)
{
    // Create the System
    System system(seed, 0);

    // set a Network Policy
    auto policy = SilentPolicy(0);
    system.sim.setNetworkPolicy(policy);

    // Create the request and send it
    system.sim.send(clientRequest(100, 1));
    system.sim.run(kForever);

    system.sim.printTrace(std::cout);

    return 0;
}

using Command = int (*)(std::uint64_t);

int main(const int argc, char* argv[])
{
    const std::map<std::string, Command> commands = {
        {"normal", normal},
        {"silent-backup", silent_backup},
        {"silent-primary", silent_primary}
    };

    if (argc < 2 || !commands.contains(argv[1]))
    {
        std::cerr << "usage tool <command>\ncommands:";
        for (const auto& [name, command] : commands)
        {
            std::cerr << ' ' << name;
        }
        std::cerr << '\n';
        return 1;
    }

    // Retrieve the argument
    constexpr std::string_view prefix = "--seed=";
    std::uint64_t seed = 0;
    if (argc == 3)
    {
        const std::string_view arg(argv[2]);
        if (arg.rfind(prefix, 0) == 0)
        {
            // Checks if arg starts with "--seed="
            std::string_view seed_string = arg.substr(prefix.size());

            try {
                // Convert the extracted string view to an integer
                seed = std::stoi(std::string(seed_string));
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid seed value '" << seed_string << "'\n";
                return 1;
            }
        }
    } else
    {
        seed = 42;
    }
    return commands.at(argv[1])(seed);
}
