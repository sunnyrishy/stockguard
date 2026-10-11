#include "stockguard/domain.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace stockguard {
Error::Error(std::string error_code, std::string message)
    : std::runtime_error(std::move(message)), code(std::move(error_code)) {}
void require(bool condition, const std::string &code, const std::string &message) {
    if (!condition)
        throw Error(code, message);
}
std::string text_field(const Json &value, const char *key, std::size_t limit) {
    require(value.is_object() && value.contains(key) && value.at(key).is_string(), "invalid_input",
            std::string(key) + " must be a string");
    const auto result = value.at(key).get<std::string>();
    require(!result.empty() && result.size() <= limit, "invalid_input",
            std::string(key) + " is empty or too long");
    require(std::none_of(result.begin(), result.end(), [](unsigned char c) { return c < 32; }),
            "invalid_input", std::string(key) + " contains control characters");
    return result;
}
std::string identifier(const Json &value, const char *key) {
    auto result = text_field(value, key);
    require(std::all_of(result.begin(), result.end(),
                        [](unsigned char c) {
                            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                   (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
                                   c == ':';
                        }),
            "invalid_input", std::string(key) + " must be an ASCII identifier");
    return result;
}
Integer integer_field(const Json &value, const char *key, Integer low, Integer high) {
    require(value.is_object() && value.contains(key) && value.at(key).is_number_integer(),
            "invalid_input", std::string(key) + " must be an integer");
    const auto &field = value.at(key);
    if (field.is_number_unsigned())
        require(field.get<std::uint64_t>() <= static_cast<std::uint64_t>(high), "invalid_input",
                std::string(key) + " is out of range");
    auto result = field.get<Integer>();
    require(result >= low && result <= high, "invalid_input",
            std::string(key) + " is out of range");
    return result;
}
Integer clock_seconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
void Policy::validate() const {
    require(std::isfinite(minimum_confidence) && minimum_confidence >= 0 &&
                minimum_confidence <= 1 && std::isfinite(relative_tolerance) &&
                relative_tolerance >= 0 && relative_tolerance <= 1 && absolute_tolerance >= 0 &&
                absolute_tolerance <= max_quantity && max_age_seconds >= 0 &&
                max_age_seconds <= 86400 && future_skew_seconds >= 0 && future_skew_seconds <= 300,
            "invalid_policy", "Policy fields are outside supported bounds");
}
Json Policy::json() const {
    return {{"minimum_confidence", minimum_confidence},
            {"absolute_tolerance", absolute_tolerance},
            {"relative_tolerance", relative_tolerance},
            {"max_age_seconds", max_age_seconds},
            {"future_skew_seconds", future_skew_seconds}};
}
Observation Observation::parse(const Json &v) {
    Observation o;
    o.id = identifier(v, "id");
    o.scan_id = identifier(v, "scan_id");
    o.product_id = identifier(v, "product_id");
    o.location_id = identifier(v, "location_id");
    o.source = text_field(v, "source");
    o.model_version = v.contains("model_version") ? text_field(v, "model_version") : "unspecified";
    o.quantity = integer_field(v, "quantity", 0, max_quantity);
    o.observed_at = integer_field(v, "observed_at", 0, 253402300799LL);
    o.expected_version = integer_field(v, "expected_version", 1, 1'000'000'000'000LL);
    require(v.contains("confidence") && v.at("confidence").is_number(), "invalid_input",
            "confidence must be numeric");
    o.confidence = v.at("confidence").get<double>();
    require(std::isfinite(o.confidence) && o.confidence >= 0 && o.confidence <= 1, "invalid_input",
            "confidence must be finite and in [0,1]");
    require(v.contains("complete") && v.at("complete").is_boolean(), "invalid_input",
            "complete must be boolean");
    o.complete = v.at("complete").get<bool>();
    o.metadata = v.value("metadata", Json::object());
    require(o.metadata.is_object() && o.metadata.dump().size() <= 4096, "invalid_input",
            "metadata must be an object of at most 4096 bytes");
    require(v.dump().size() <= 16384, "invalid_input", "Observation too large");
    return o;
}
std::string to_string(Status s) {
    switch (s) {
    case Status::Accept:
        return "ACCEPT";
    case Status::Review:
        return "REVIEW";
    case Status::Reject:
        return "REJECT";
    case Status::Defer:
        return "DEFER";
    }
    throw Error("internal", "Unknown status");
}
Json Decision::json() const {
    return {{"status", to_string(status)},
            {"reason", reason},
            {"explanation", explanation},
            {"evidence", evidence}};
}
Decision evaluate(const Observation &o, bool active, const std::optional<Inventory> &state,
                  const Policy &p, Integer now) {
    p.validate();
    require(o.quantity >= 0 && o.quantity <= max_quantity && std::isfinite(o.confidence) &&
                o.confidence >= 0 && o.confidence <= 1 && o.observed_at >= 0 &&
                o.observed_at <= 253402300799LL && o.expected_version >= 1 &&
                o.expected_version <= 1'000'000'000'000LL && now >= 0 && now <= 253402300799LL,
            "invalid_input", "Invalid observation scalar or evaluation clock");
    if (state)
        require(state->quantity >= 0 && state->quantity <= max_quantity && state->version >= 1 &&
                    state->version <= 1'000'000'000'000LL && state->updated_at >= 0 &&
                    state->updated_at <= 253402300799LL,
                "invalid_inventory", "Invalid inventory state");
    Json evidence = {{"observed_quantity", o.quantity},
                     {"confidence", o.confidence},
                     {"complete", o.complete},
                     {"observed_at", o.observed_at},
                     {"expected_version", o.expected_version},
                     {"policy", p.json()},
                     {"now", now}};
    if (state) {
        evidence["inventory_quantity"] = state->quantity;
        evidence["inventory_version"] = state->version;
        evidence["inventory_updated_at"] = state->updated_at;
        evidence["inventory_source"] = state->source;
        evidence["discrepancy"] = std::abs(o.quantity - state->quantity);
    }
    auto result = [&](Status s, std::string r, std::string explanation) {
        return Decision{s, std::move(r), std::move(explanation), evidence};
    };
    if (!active)
        return result(Status::Reject, "unknown_or_inactive_product",
                      "Product cannot be reconciled");
    if (!state)
        return result(Status::Defer, "missing_inventory", "Initialize trusted inventory first");
    if (o.observed_at > now + p.future_skew_seconds)
        return result(Status::Reject, "future_observation",
                      "Observation exceeds future clock tolerance");
    if (now - o.observed_at > p.max_age_seconds || o.observed_at < state->updated_at)
        return result(Status::Reject, "stale_observation",
                      "Observation predates current ledger or freshness window");
    if (o.expected_version != state->version)
        return result(Status::Defer, "version_conflict",
                      "Rescan against the current inventory version");
    if (!o.complete)
        return result(Status::Defer, "partial_count",
                      "A complete product-location count is required");
    if (o.confidence < p.minimum_confidence)
        return result(Status::Review, "low_confidence",
                      "Human verification required for uncertain classification");
    double tolerance = std::max(static_cast<double>(p.absolute_tolerance),
                                p.relative_tolerance * static_cast<double>(state->quantity));
    if (static_cast<double>(std::abs(o.quantity - state->quantity)) > tolerance)
        return result(Status::Review, "large_discrepancy",
                      "Count differs from the movement-adjusted ledger");
    return result(Status::Accept, "consistent_count",
                  "Complete fresh count satisfies all configured checks");
}
} // namespace stockguard
