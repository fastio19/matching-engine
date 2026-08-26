#include "service/MatchingService.h"

#include <type_traits>
#include <utility>
#include <stdexcept>

MatchingService::~MatchingService()
{
    stop();
}

void MatchingService::start()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (running)
    {
        return;
    }

    stopping = false;
    running = true;
    worker = std::thread(&MatchingService::run, this);
}

void MatchingService::stop()
{
    std::thread localWorker;

    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!running)
        {
            return;
        }

        stopping = true;
        running = false;
        localWorker = std::move(worker);
    }

    commandCv.notify_all();

    if (localWorker.joinable())
    {
        localWorker.join();
    }
}

void MatchingService::submitOrder(const Order& order)
{
    enqueue(PlaceOrderCommand{order});
}

void MatchingService::submitCancel(OrderId orderId)
{
    enqueue(CancelOrderCommand{orderId});
}

void MatchingService::waitUntilIdle()
{
    std::unique_lock<std::mutex> lock(mutex);
    idleCv.wait(lock, [this]
    {
        return commands.empty() && activeCommands == 0;
    });
}

bool MatchingService::isRunning() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return running;
}

MatchingEngine& MatchingService::engine()
{
    return matchingEngine;
}

const MatchingEngine& MatchingService::engine() const
{
    return matchingEngine;
}

void MatchingService::enqueue(MatchingCommand command)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!running)
        {
            throw std::runtime_error("MatchingService is not running");
        }

        commands.push(std::move(command));
    }

    commandCv.notify_one();
}

void MatchingService::run()
{
    while (true)
    {
        MatchingCommand command = PlaceOrderCommand{Order{}};

        {
            std::unique_lock<std::mutex> lock(mutex);
            commandCv.wait(lock, [this]
            {
                return stopping || !commands.empty();
            });

            if (stopping && commands.empty())
            {
                idleCv.notify_all();
                break;
            }

            command = std::move(commands.front());
            commands.pop();
            ++activeCommands;
        }

        std::visit([this](auto&& item)
        {
            using CommandType = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<CommandType, PlaceOrderCommand>)
            {
                matchingEngine.processOrder(item.order);
            }
            else if constexpr (std::is_same_v<CommandType, CancelOrderCommand>)
            {
                matchingEngine.cancelOrder(item.orderId);
            }
        }, command);

        {
            std::lock_guard<std::mutex> lock(mutex);
            --activeCommands;
            if (commands.empty() && activeCommands == 0)
            {
                idleCv.notify_all();
            }
        }
    }
}
