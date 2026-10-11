#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>

namespace stockguard {
using Json = nlohmann::json;
using Integer = std::int64_t;
inline constexpr Integer max_quantity = 1'000'000'000;
class Error : public std::runtime_error {
  public:
    Error(std::string code, std::string message);
    std::string code;
};
enum class Status { Accept, Review, Reject, Defer };
std::string to_string(Status status);
struct Policy {
    double minimum_confidence = 0.95;
    Integer absolute_tolerance = 2;
    double relative_tolerance = 0.05;
    Integer max_age_seconds = 300;
    Integer future_skew_seconds = 5;
    void validate() const;
    Json json() const;
};
struct Inventory {
    Integer quantity;
    Integer version;
    Integer updated_at;
    std::string source;
};
struct Observation {
    std::string id, scan_id, product_id, location_id, source, model_version;
    Integer quantity, observed_at, expected_version;
    double confidence;
    bool complete;
    Json metadata;
    static Observation parse(const Json &value);
};
struct Decision {
    Status status;
    std::string reason, explanation;
    Json evidence;
    Json json() const;
};
Decision evaluate(const Observation &observation, bool active_product,
                  const std::optional<Inventory> &inventory, const Policy &policy, Integer now);
std::string text_field(const Json &value, const char *key, std::size_t limit = 128);
std::string identifier(const Json &value, const char *key);
Integer integer_field(const Json &value, const char *key, Integer low, Integer high);
Integer clock_seconds();
void require(bool condition, const std::string &code, const std::string &message);
} // namespace stockguard
