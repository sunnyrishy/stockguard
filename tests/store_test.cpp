#include "stockguard/store.hpp"
#include <atomic>
#include <filesystem>
#include <gtest/gtest.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>
using namespace stockguard;
namespace {
std::atomic<int> counter{0};
class Persistence : public ::testing::Test {
  protected:
    std::string path =
        (std::filesystem::temp_directory_path() /
         ("stockguard-test-" + std::to_string(getpid()) + "-" + std::to_string(counter++) + ".db"))
            .string();
    void TearDown() override {
        for (auto suffix : {"", "-wal", "-shm"})
            std::filesystem::remove(path + suffix);
    }
    static Integer now() { return 1000; }
    void seed(Store &s) {
        s.execute({{"op", "product_put"},
                   {"id", "milk"},
                   {"name", "Whole milk"},
                   {"category", "dairy"},
                   {"unit", "carton"},
                   {"status", "active"}});
        s.execute({{"op", "initialize"},
                   {"product_id", "milk"},
                   {"location_id", "store"},
                   {"quantity", 100},
                   {"actor", "seed"}});
    }
    Json observation(std::string id = "o1", Integer quantity = 101, Integer version = 1) {
        return {{"id", id},
                {"scan_id", "scan-" + id},
                {"product_id", "milk"},
                {"location_id", "store"},
                {"source", "synthetic"},
                {"quantity", quantity},
                {"confidence", 0.99},
                {"observed_at", 1000},
                {"expected_version", version},
                {"complete", true}};
    }
    Json ingest(Store &s, Json o) { return s.execute({{"op", "ingest"}, {"observation", o}}); }
    Json state(Store &s) { return s.execute({{"op", "inventory"}}).at(0); }
    Json correction(std::string action = "correct", Integer version = 1) {
        return {{"op", "review"},     {"observation_id", "o1"}, {"actor", "reviewer"},
                {"action", action},   {"quantity", 98},         {"expected_version", version},
                {"verified_at", 1000}};
    }
};
TEST_F(Persistence, VerticalSliceAndAudit) {
    Store s(path, {}, now);
    seed(s);
    auto d = ingest(s, observation());
    EXPECT_EQ(d["status"], "ACCEPT");
    EXPECT_EQ(state(s)["quantity"], 101);
    EXPECT_EQ(state(s)["version"], 2);
    auto audit = s.execute({{"op", "audit"}});
    ASSERT_EQ(audit.size(), 2);
    EXPECT_EQ(audit[1]["previous_quantity"], 100);
    EXPECT_EQ(audit[1]["event_id"], "o1");
    EXPECT_EQ(s.execute({{"op", "metrics"}})["acceptance_rate"], 1.0);
}
TEST_F(Persistence, CatalogUpdateRetrieveAndValidation) {
    Store s(path, {}, now);
    seed(s);
    EXPECT_EQ(s.execute({{"op", "products"}, {"id", "milk"}})[0]["name"], "Whole milk");
    s.execute({{"op", "product_put"},
               {"id", "milk"},
               {"name", "Milk"},
               {"category", "dairy"},
               {"unit", "carton"},
               {"status", "inactive"}});
    EXPECT_EQ(ingest(s, observation())["status"], "REJECT");
    EXPECT_EQ(state(s)["version"], 1);
    EXPECT_THROW(s.execute({{"op", "initialize"},
                            {"product_id", "milk"},
                            {"location_id", "x"},
                            {"quantity", 5},
                            {"actor", "seed"}}),
                 Error);
}
TEST_F(Persistence, ReplaySurvivesRestartAndPayloadConflict) {
    Json original;
    {
        Store s(path, {}, now);
        seed(s);
        original = ingest(s, observation());
    }
    Store s(path, {}, now);
    auto replay = ingest(s, observation());
    EXPECT_EQ(replay["replayed"], true);
    replay["replayed"] = false;
    EXPECT_EQ(replay, original);
    EXPECT_THROW(ingest(s, observation("o1", 102)), Error);
    EXPECT_EQ(state(s)["version"], 2);
    EXPECT_EQ(s.execute({{"op", "audit"}}).size(), 2);
}
TEST_F(Persistence, AlternateIdScanIsRejected) {
    Store s(path, {}, now);
    seed(s);
    ingest(s, observation());
    auto second = observation("o2", 102, 2);
    second["scan_id"] = "scan-o1";
    EXPECT_EQ(ingest(s, second)["reason"], "duplicate_scan");
    EXPECT_EQ(state(s)["quantity"], 101);
}
TEST_F(Persistence, MovementIsTransactionalAndReplaySafe) {
    Store s(path, {}, now);
    seed(s);
    Json m = {{"op", "movement"},       {"id", "m1"},   {"product_id", "milk"},
              {"location_id", "store"}, {"delta", -10}, {"expected_version", 1},
              {"actor", "sale"}};
    EXPECT_EQ(s.execute(m)["quantity"], 90);
    EXPECT_EQ(s.execute(m)["replayed"], true);
    EXPECT_EQ(ingest(s, observation())["reason"], "version_conflict");
    EXPECT_EQ(ingest(s, observation("o2", 90, 2))["status"], "ACCEPT");
    m["id"] = "m2";
    m["expected_version"] = 3;
    m["delta"] = -100;
    EXPECT_THROW(s.execute(m), Error);
    EXPECT_EQ(state(s)["quantity"], 90);
    EXPECT_EQ(s.execute({{"op", "audit"}}).size(), 3);
}
TEST_F(Persistence, ReviewCorrectionIsAuditedAndIdempotent) {
    Store s(path, {}, now);
    seed(s);
    EXPECT_EQ(ingest(s, observation("o1", 160))["status"], "REVIEW");
    EXPECT_EQ(state(s)["quantity"], 100);
    EXPECT_EQ(s.execute({{"op", "pending"}}).size(), 1);
    auto r = correction();
    EXPECT_EQ(s.execute(r)["quantity"], 98);
    EXPECT_EQ(s.execute(r)["replayed"], true);
    r["quantity"] = 99;
    EXPECT_THROW(s.execute(r), Error);
    EXPECT_EQ(s.execute({{"op", "pending"}}).size(), 0);
    EXPECT_EQ(s.execute({{"op", "reviews"}}).size(), 1);
    EXPECT_EQ(s.execute({{"op", "audit"}})[1]["kind"], "review");
}
TEST_F(Persistence, ReviewVersionAndFreshnessConflictPreservesState) {
    Store s(path, {}, now);
    seed(s);
    ingest(s, observation("o1", 160));
    EXPECT_THROW(s.execute(correction("correct", 2)), Error);
    auto r = correction();
    r["verified_at"] = 600;
    EXPECT_THROW(s.execute(r), Error);
    r = correction();
    r["quantity"] = -1;
    EXPECT_THROW(s.execute(r), Error);
    EXPECT_EQ(state(s)["version"], 1);
    EXPECT_EQ(s.execute({{"op", "pending"}}).size(), 1);
}
TEST_F(Persistence, RejectAndConfirmReview) {
    Store s(path, {}, now);
    seed(s);
    ingest(s, observation("o1", 160));
    s.execute(correction("reject"));
    EXPECT_EQ(state(s)["version"], 1);
    auto o = observation("o2", 99);
    o["confidence"] = 0.5;
    ingest(s, o);
    auto r = correction("confirm");
    r["observation_id"] = "o2";
    s.execute(r);
    EXPECT_EQ(state(s)["quantity"], 99);
    EXPECT_EQ(s.execute({{"op", "audit"}}).size(), 2);
}
TEST_F(Persistence, BatchPartialSuccessAndBound) {
    Store s(path, {}, now);
    seed(s);
    auto r = s.execute({{"op", "batch"},
                        {"batch_id", "b1"},
                        {"observations",
                         Json::array({observation(), Json::object(), observation("o2", 102, 2)})}});
    ASSERT_EQ(r["results"].size(), 3);
    EXPECT_EQ(r["results"][0]["ok"], true);
    EXPECT_EQ(r["results"][1]["ok"], false);
    EXPECT_EQ(r["results"][2]["ok"], true);
    EXPECT_EQ(state(s)["version"], 3);
    EXPECT_THROW(s.execute({{"op", "batch"}, {"batch_id", "b2"}, {"observations", Json::array()}}),
                 Error);
}
TEST_F(Persistence, RollbackAtEveryObservationCheckpoint) {
    {
        Store s(path, {}, now);
        seed(s);
    }
    for (auto point : {"after_observation", "after_inventory", "before_commit"}) {
        Store s(path, {}, now, [&](const std::string &p) {
            if (p == point)
                throw Error("injected", "failure");
        });
        EXPECT_THROW(ingest(s, observation()), Error);
        Store inspect(path, {}, now);
        EXPECT_EQ(state(inspect)["quantity"], 100);
        EXPECT_EQ(inspect.execute({{"op", "history"}}).size(), 0);
        EXPECT_EQ(inspect.execute({{"op", "audit"}}).size(), 1);
    }
    Store s(path, {}, now);
    EXPECT_EQ(ingest(s, observation())["status"], "ACCEPT");
}
TEST_F(Persistence, ReviewRollbackLeavesPendingEvent) {
    {
        Store s(path, {}, now);
        seed(s);
        ingest(s, observation("o1", 160));
    }
    Store s(path, {}, now, [](const std::string &p) {
        if (p == "after_inventory")
            throw Error("injected", "failure");
    });
    EXPECT_THROW(s.execute(correction()), Error);
    Store inspect(path, {}, now);
    EXPECT_EQ(state(inspect)["quantity"], 100);
    EXPECT_EQ(inspect.execute({{"op", "pending"}}).size(), 1);
    EXPECT_EQ(inspect.execute({{"op", "reviews"}}).size(), 0);
}
TEST_F(Persistence, ProcessDeathRollsBackAndReplayRecovers) {
    {
        Store s(path, {}, now);
        seed(s);
    }
    const auto pid = fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        Store child(path, {}, now, [](const std::string &p) {
            if (p == "after_inventory")
                _exit(73);
        });
        ingest(child, observation());
        _exit(74);
    }
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 73);
    Store s(path, {}, now);
    EXPECT_EQ(state(s)["quantity"], 100);
    EXPECT_EQ(s.execute({{"op", "history"}}).size(), 0);
    EXPECT_EQ(ingest(s, observation())["status"], "ACCEPT");
}
TEST_F(Persistence, ConcurrentDuplicatesAcrossConnections) {
    {
        Store s(path, {}, now);
        seed(s);
    }
    std::atomic<int> accepted{0}, failed{0};
    std::vector<std::jthread> threads;
    for (int i = 0; i < 12; ++i)
        threads.emplace_back([&] {
            try {
                Store s(path, {}, now);
                ingest(s, observation());
                ++accepted;
            } catch (...) {
                ++failed;
            }
        });
    threads.clear();
    EXPECT_EQ(accepted, 12);
    EXPECT_EQ(failed, 0);
    Store s(path, {}, now);
    EXPECT_EQ(state(s)["version"], 2);
    EXPECT_EQ(s.execute({{"op", "audit"}}).size(), 2);
}
TEST_F(Persistence, ConcurrentDistinctEventsCannotOverwriteSameVersion) {
    {
        Store s(path, {}, now);
        seed(s);
    }
    std::atomic<int> accepted{0}, deferred{0}, failed{0};
    std::vector<std::jthread> threads;
    for (int i = 0; i < 12; ++i)
        threads.emplace_back([&, i] {
            try {
                Store s(path, {}, now);
                auto d = ingest(s, observation("o" + std::to_string(i)));
                if (d["status"] == "ACCEPT")
                    ++accepted;
                else if (d["status"] == "DEFER")
                    ++deferred;
            } catch (...) {
                ++failed;
            }
        });
    threads.clear();
    EXPECT_EQ(accepted, 1);
    EXPECT_EQ(deferred, 11);
    EXPECT_EQ(failed, 0);
    Store s(path, {}, now);
    EXPECT_EQ(state(s)["version"], 2);
}
TEST_F(Persistence, ConcurrentConflictingReviewsOnlyOneCommits) {
    {
        Store s(path, {}, now);
        seed(s);
        ingest(s, observation("o1", 160));
    }
    std::atomic<int> success{0}, conflict{0};
    std::vector<std::jthread> threads;
    for (int i = 0; i < 8; ++i)
        threads.emplace_back([&, i] {
            try {
                Store s(path, {}, now);
                auto r = correction();
                r["actor"] = "reviewer" + std::to_string(i);
                s.execute(r);
                ++success;
            } catch (const Error &e) {
                if (e.code == "review_conflict")
                    ++conflict;
            }
        });
    threads.clear();
    EXPECT_EQ(success, 1);
    EXPECT_EQ(conflict, 7);
    Store s(path, {}, now);
    EXPECT_EQ(state(s)["version"], 2);
    EXPECT_EQ(s.execute({{"op", "audit"}}).size(), 2);
}
TEST_F(Persistence, ForeignKeysAndFutureSchemaAreEnforced) {
    {
        Store s(path, {}, now);
        seed(s);
    }
    Database db(path);
    EXPECT_THROW(db.exec("INSERT INTO inventory VALUES('missing','store',1,1,1000,'test')"), Error);
    db.exec("PRAGMA user_version=2");
    EXPECT_THROW(Store(path, {}, now), Error);
}
TEST_F(Persistence, PaginationAndMalformedOperation) {
    Store s(path, {}, now);
    seed(s);
    ingest(s, observation());
    EXPECT_EQ(s.execute({{"op", "audit"}, {"limit", 1}, {"offset", 1}}).size(), 1);
    EXPECT_THROW(s.execute({{"op", "audit"}, {"limit", 0}}), Error);
    EXPECT_THROW(s.execute({{"op", "unknown"}}), Error);
}

TEST_F(Persistence, MovementRollbackAndTimestampStaleness) {
    {
        Store s(path, {}, now);
        seed(s);
    }
    Json m = {{"op", "movement"},       {"id", "m1"},   {"product_id", "milk"},
              {"location_id", "store"}, {"delta", -10}, {"expected_version", 1},
              {"actor", "sale"}};
    for (const auto &point : {"after_inventory", "before_commit"}) {
        Store fault(path, {}, now, [&](const std::string &p) {
            if (p == point)
                throw Error("injected", "failure");
        });
        EXPECT_THROW(fault.execute(m), Error);
        Store inspect(path, {}, now);
        EXPECT_EQ(state(inspect)["quantity"], 100);
        EXPECT_EQ(inspect.execute({{"op", "audit"}}).size(), 1);
    }
    Store later(path, {}, [] { return 1010; });
    later.execute(m);
    auto o = observation("o1", 90, 2);
    EXPECT_EQ(ingest(later, o)["reason"], "stale_observation");
    EXPECT_EQ(state(later)["quantity"], 90);
}
TEST_F(Persistence, SharedStoreMutexAndRandomizedLedgerInvariants) {
    Store s(path, {}, now);
    seed(s);
    std::vector<std::jthread> threads;
    std::atomic<int> errors{0};
    for (int i = 0; i < 8; ++i)
        threads.emplace_back([&] {
            try {
                ingest(s, observation());
            } catch (...) {
                ++errors;
            }
        });
    threads.clear();
    EXPECT_EQ(errors, 0);
    EXPECT_EQ(state(s)["version"], 2);
    Integer expected_quantity = 101, version = 2;
    for (Integer i = 0; i < 100; ++i) {
        Integer delta = i % 2 == 0 ? 3 : -2;
        s.execute({{"op", "movement"},
                   {"id", "m" + std::to_string(i)},
                   {"product_id", "milk"},
                   {"location_id", "store"},
                   {"actor", "test"},
                   {"expected_version", version++},
                   {"delta", delta}});
        expected_quantity += delta;
        EXPECT_EQ(state(s)["quantity"], expected_quantity);
        EXPECT_EQ(state(s)["version"], version);
    }
    auto audit = s.execute({{"op", "audit"}});
    ASSERT_EQ(audit.size(), 102);
    for (std::size_t i = 1; i < audit.size(); ++i)
        EXPECT_EQ(audit[i]["previous_quantity"], audit[i - 1]["new_quantity"]);
}

TEST_F(Persistence, CatalogUnitCannotReinterpretExistingInventory) {
    Store s(path, {}, now);
    seed(s);
    EXPECT_THROW(s.execute({{"op", "product_put"},
                            {"id", "milk"},
                            {"name", "Milk"},
                            {"category", "dairy"},
                            {"unit", "litre"},
                            {"status", "active"}}),
                 Error);
    EXPECT_EQ(s.execute({{"op", "products"}, {"id", "milk"}})[0]["unit"], "carton");
    EXPECT_EQ(state(s)["quantity"], 100);
}
} // namespace
