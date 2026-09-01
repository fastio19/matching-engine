#include "marketdata/kafka/KafkaMatchingRunner.h"

#include <utility>

#include "marketdata/kafka/KafkaSchema.h"
#include "marketdata/kafka/KafkaTradePublishing.h"

KafkaMatchingRunner::KafkaMatchingRunner(const std::string& brokers,
                                         const std::string& commandTopic,
                                         const std::string& consumerGroup,
                                         const std::string& tradeTopic,
                                         const std::string& bookTopic,
                                         const std::string& udpMulticastGroup,
                                         uint16_t udpMulticastPort,
                                         const std::string& recoverySnapshotPath)
    : matchingEngine(),
      commandConsumer(brokers, commandTopic, consumerGroup),
      tradePublisher(std::make_unique<KafkaPublisher>(brokers, tradeTopic)),
      bookPublisher(std::make_unique<KafkaPublisher>(brokers, bookTopic)),
      udpPublisher(std::make_unique<UDPPublisher>(udpMulticastGroup, udpMulticastPort)),
      stateStore(recoverySnapshotPath),
      running(false)
{
    matchingEngine.restoreOpenOrders(stateStore.loadOpenOrders());
    wireHandlers();
}

KafkaMatchingRunner::~KafkaMatchingRunner()
{
    stop();
}

void KafkaMatchingRunner::start()
{
    if (running.exchange(true))
    {
        return;
    }

    consumerThread = std::thread([this]
    {
        commandConsumer.start();
    });
}

void KafkaMatchingRunner::stop()
{
    if (!running.exchange(false))
    {
        return;
    }

    commandConsumer.stop();

    if (consumerThread.joinable())
    {
        consumerThread.join();
    }
}

MatchingEngine& KafkaMatchingRunner::engine()
{
    return matchingEngine;
}

const MatchingEngine& KafkaMatchingRunner::engine() const
{
    return matchingEngine;
}

void KafkaMatchingRunner::wireHandlers()
{
    wireTradePublishing(matchingEngine, *tradePublisher);
    wireLastTradedPricePublishing(matchingEngine, *udpPublisher);
    wireBookPublishing(matchingEngine, *bookPublisher);

    commandConsumer.setPlaceHandler([this](const Order& order)
    {
        matchingEngine.processOrder(order);
        saveStateSnapshot();
    });

    commandConsumer.setModifyHandler([this](const Order& order)
    {
        matchingEngine.cancelOrder(order.id);
        matchingEngine.processOrder(order);
        saveStateSnapshot();
    });

    commandConsumer.setCancelHandler([this](OrderId orderId, uint32_t)
    {
        matchingEngine.cancelOrder(orderId);
        saveStateSnapshot();
    });
}

void KafkaMatchingRunner::saveStateSnapshot() const
{
    stateStore.saveOpenOrders(matchingEngine.getOpenOrders());
}
