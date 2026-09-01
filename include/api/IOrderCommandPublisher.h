#pragma once

#include "marketdata/kafka/KafkaSchema.h"

class IOrderCommandPublisher {
public:
    virtual ~IOrderCommandPublisher() = default;

    virtual void publishOrderCommand(const KafkaOrderCommandMessage& message) = 0;
};
