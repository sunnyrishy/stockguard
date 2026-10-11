#pragma once
#include "stockguard/database.hpp"
#include <functional>
#include <mutex>

namespace stockguard {
// Test hooks are not exposed through the executable or dashboard.
class Store {
  public:
    using Clock = std::function<Integer()>;
    using Checkpoint = std::function<void(const std::string &)>;
    explicit Store(const std::string &path, Policy policy = {}, Clock clock = clock_seconds,
                   Checkpoint checkpoint = {});
    Json execute(const Json &request);

  private:
    void migrate();
    Json execute_locked(const Json &request);
    Json ingest(const Json &observation);
    Json review(const Json &request);
    Json movement(const Json &request);
    std::optional<Inventory> inventory(const std::string &product, const std::string &location);
    bool active_product(const std::string &product);
    void audit(const std::string &product, const std::string &location, Integer previous,
               Integer next, Integer version, const std::string &kind, const std::string &event,
               const std::string &actor, Integer now);
    void update_inventory(const std::string &product, const std::string &location,
                          const Inventory &previous, Integer next, Integer timestamp,
                          const std::string &source);
    void checkpoint(const std::string &name);
    Database database_;
    Policy policy_;
    Clock clock_;
    Checkpoint checkpoint_;
    std::mutex mutex_;
};
} // namespace stockguard
