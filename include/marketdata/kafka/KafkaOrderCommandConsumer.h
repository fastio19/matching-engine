#pragma once

#include <atomic>
#include <functional>
#include <string>

#include "core/Order.h"

struct rd_kafka_s;
typedef struct rd_kafka_s rd_kafka_t;

class KafkaOrderCommandConsumer {
public:
    using PlaceHandler = std::function<void(const Order&)>;
    using ModifyHandler = std::function<void(const Order&)>;
    using CancelHandler = std::function<void(OrderId, uint32_t)>;
    using DlqHandler = std::function<void(const std::string&, const std::string&)>;

    KafkaOrderCommandConsumer(const std::string& brokers,
                              const std::string& topic,
                              const std::string& consumerGroup);
    ~KafkaOrderCommandConsumer();

    KafkaOrderCommandConsumer(const KafkaOrderCommandConsumer&) = delete;
    KafkaOrderCommandConsumer& operator=(const KafkaOrderCommandConsumer&) = delete;

    void start();
    void stop();

    void setPlaceHandler(PlaceHandler handler);
    void setModifyHandler(ModifyHandler handler);
    void setCancelHandler(CancelHandler handler);
    void setDlqHandler(DlqHandler handler);

private:
    std::string brokerList;
    std::string topicName;
    std::string groupId;

    rd_kafka_t* consumer;
    std::atomic<bool> running;

    PlaceHandler placeHandler;
    ModifyHandler modifyHandler;
    CancelHandler cancelHandler;
    DlqHandler dlqHandler;

    void initConsumer();
    void subscribe();
    void consumeLoop();
    void processMessage(const std::string& payload);
    void sendToDlq(const std::string& payload, const std::string& reason);
};
