#include "stockguard/domain.hpp"
#include <gtest/gtest.h>
#include <limits>
using namespace stockguard;
namespace {
Json sample() {
    return {{"id", "o1"},
            {"scan_id", "s1"},
            {"product_id", "milk"},
            {"location_id", "store"},
            {"source", "synthetic"},
            {"quantity", 100},
            {"confidence", 0.99},
            {"observed_at", 1000},
            {"expected_version", 1},
            {"complete", true}};
}
Decision decision(Json j,
                  std::optional<Inventory> state = Inventory{100, 1, 1000, "initialization"},
                  bool active = true, Integer now = 1000) {
    return evaluate(Observation::parse(j), active, state, Policy{}, now);
}
} // namespace
TEST(Engine, AcceptsConsistentCompleteCount) {
    EXPECT_EQ(decision(sample()).status, Status::Accept);
    EXPECT_EQ(decision(sample()).reason, "consistent_count");
}
TEST(Engine, ConfidenceBoundary) {
    auto j = sample();
    j["confidence"] = 0.95;
    EXPECT_EQ(decision(j).status, Status::Accept);
    j["confidence"] = 0.949;
    EXPECT_EQ(decision(j).reason, "low_confidence");
}
TEST(Engine, AbsoluteAndRelativeDiscrepancyBoundaries) {
    auto j = sample();
    j["quantity"] = 105;
    EXPECT_EQ(decision(j).status, Status::Accept);
    j["quantity"] = 106;
    EXPECT_EQ(decision(j).reason, "large_discrepancy");
    j["quantity"] = 2;
    EXPECT_EQ(decision(j, Inventory{0, 1, 1000, "init"}).status, Status::Accept);
    j["quantity"] = 3;
    EXPECT_EQ(decision(j, Inventory{0, 1, 1000, "init"}).status, Status::Review);
}
TEST(Engine, UnknownProductAndMissingInventory) {
    EXPECT_EQ(decision(sample(), std::nullopt, false).status, Status::Reject);
    EXPECT_EQ(decision(sample(), std::nullopt, true).status, Status::Defer);
}
TEST(Engine, FreshnessAndClockBoundaries) {
    auto j = sample();
    j["observed_at"] = 700;
    EXPECT_EQ(decision(j, Inventory{100, 1, 700, "init"}).status, Status::Accept);
    j["observed_at"] = 699;
    EXPECT_EQ(decision(j).reason, "stale_observation");
    j["observed_at"] = 1005;
    EXPECT_EQ(decision(j).status, Status::Accept);
    j["observed_at"] = 1006;
    EXPECT_EQ(decision(j).reason, "future_observation");
}
TEST(Engine, PartialAndVersionConflictDoNotAccept) {
    auto j = sample();
    j["complete"] = false;
    EXPECT_EQ(decision(j).reason, "partial_count");
    j["expected_version"] = 2;
    EXPECT_EQ(decision(j).reason, "version_conflict");
}
TEST(Engine, RejectsMalformedFields) {
    for (auto key : {"id", "scan_id", "product_id", "location_id", "source", "quantity",
                     "observed_at", "expected_version", "confidence", "complete"}) {
        auto j = sample();
        j.erase(key);
        EXPECT_THROW(Observation::parse(j), Error) << key;
    }
    for (auto qty : {Json(-1), Json(1.5), Json(max_quantity + 1),
                     Json(std::numeric_limits<std::uint64_t>::max())}) {
        auto j = sample();
        j["quantity"] = qty;
        EXPECT_THROW(Observation::parse(j), Error);
    }
    auto j = sample();
    j["confidence"] = std::numeric_limits<double>::infinity();
    EXPECT_THROW(Observation::parse(j), Error);
    j = sample();
    j["confidence"] = -0.1;
    EXPECT_THROW(Observation::parse(j), Error);
    j = sample();
    j["id"] = "bad\nidentifier";
    EXPECT_THROW(Observation::parse(j), Error);
    j = sample();
    j["metadata"] = Json::array();
    EXPECT_THROW(Observation::parse(j), Error);
}
TEST(Engine, RejectsInvalidPolicy) {
    Policy p;
    p.minimum_confidence = 1.1;
    EXPECT_THROW(p.validate(), Error);
    p = {};
    p.relative_tolerance = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(p.validate(), Error);
    p = {};
    p.max_age_seconds = -1;
    EXPECT_THROW(p.validate(), Error);
}
TEST(Engine, SeededBoundaryInvariants) {
    for (Integer qty = 0; qty < 250; ++qty) {
        auto j = sample();
        j["quantity"] = qty;
        auto d = decision(j);
        EXPECT_EQ(d.status == Status::Accept, qty >= 95 && qty <= 105);
        EXPECT_FALSE(d.explanation.empty());
        EXPECT_TRUE(d.evidence.contains("inventory_version"));
    }
}

TEST(Engine, DirectValueApiProtectsArithmeticBounds) {
    auto o = Observation::parse(sample());
    o.quantity = std::numeric_limits<Integer>::max();
    EXPECT_THROW(evaluate(o, true, Inventory{100, 1, 1000, "init"}, Policy{}, 1000), Error);
    o = Observation::parse(sample());
    EXPECT_THROW(evaluate(o, true, Inventory{-1, 1, 1000, "init"}, Policy{}, 1000), Error);
    EXPECT_THROW(evaluate(o, true, Inventory{100, 1, 1000, "init"}, Policy{}, -1), Error);
}
