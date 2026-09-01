#pragma once

#include <string>
#include <vector>

#include "core/Order.h"

class FileOrderStateStore {
public:
    explicit FileOrderStateStore(std::string snapshotPath);

    std::vector<Order> loadOpenOrders() const;
    void saveOpenOrders(const std::vector<Order>& orders) const;

private:
    std::string snapshotPath;
};
