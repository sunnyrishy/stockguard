#include "stockguard/store.hpp"
#include <algorithm>
#include <barrier>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <sys/resource.h>
#include <thread>

using namespace stockguard;
namespace {
using Clock = std::chrono::steady_clock;
struct Quality {
    Integer tp = 0, fp = 0, fn = 0, tn = 0, manual = 0, committed_wrong = 0;
    void add(bool bad, bool accept, bool verify = false) {
        if (bad && !accept)
            ++tp;
        if (!bad && !accept)
            ++fp;
        if (bad && accept)
            ++fn;
        if (!bad && accept)
            ++tn;
        if (!accept)
            ++manual;
        if (bad && accept && !verify)
            ++committed_wrong;
    }
    Json json() const {
        return {{"tp", tp},
                {"fp", fp},
                {"fn", fn},
                {"tn", tn},
                {"precision", tp + fp ? static_cast<double>(tp) / static_cast<double>(tp + fp) : 0},
                {"recall", tp + fn ? static_cast<double>(tp) / static_cast<double>(tp + fn) : 0},
                {"review_or_recount_workload", manual},
                {"incorrect_updates_committed", committed_wrong},
                {"simulated_verification_seconds", manual * 15}};
    }
};
Json observation(Integer i, Integer quantity, Integer version) {
    return {{"id", "o" + std::to_string(i)},
            {"scan_id", "s" + std::to_string(i)},
            {"product_id", "milk"},
            {"location_id", "store"},
            {"source", "synthetic"},
            {"quantity", quantity},
            {"confidence", 0.99},
            {"observed_at", 1000},
            {"expected_version", version},
            {"complete", true}};
}
void seed(Store &s) {
    s.execute({{"op", "product_put"},
               {"id", "milk"},
               {"name", "Milk"},
               {"category", "dairy"},
               {"unit", "carton"},
               {"status", "active"}});
    s.execute({{"op", "initialize"},
               {"product_id", "milk"},
               {"location_id", "store"},
               {"quantity", 100},
               {"actor", "seed"}});
}
} // namespace
int main(int argc, char **argv) {
    try {
        Integer n = argc > 1 ? std::stoll(argv[1]) : 10000;
        require(n >= 100 && n <= 1'000'000, "invalid_input", "Size must be 100 to 1000000");
        Integer workers = argc > 3 ? std::stoll(argv[3]) : 1;
        require(workers >= 1 && workers <= 16, "invalid_input", "Workers must be 1 to 16");
        std::string path = argc > 2 ? argv[2] : "benchmark.db";
        require(!std::filesystem::exists(path), "already_exists",
                "Benchmark needs a new database path");
        std::mt19937 random(2026);
        Quality blind, verify, confidence, guard;
        Json examples = Json::array();
        std::vector<Observation> dataset;
        dataset.reserve(static_cast<std::size_t>(n));
        auto quality_start = Clock::now();
        for (Integer i = 0; i < n; ++i) {
            // Independent snapshots avoid changing the ground truth for competing policies.
            unsigned scenario = random() % 10;
            auto raw = observation(i, 100, 1);
            bool bad = scenario < 5;
            if (scenario == 0)
                raw["quantity"] = 160;
            if (scenario == 1) {
                raw["quantity"] = 0;
                raw["confidence"] = 0.5;
            }
            if (scenario == 2) {
                raw["quantity"] = 80;
                raw["complete"] = false;
            }
            if (scenario == 3) {
                raw["quantity"] = 90;
                raw["observed_at"] = 600;
            }
            if (scenario == 4)
                raw["quantity"] = 102; // Plausible but wrong, unavoidable false negative.
            if (scenario == 5)
                raw["confidence"] = 0.6; // Correct but uncertain, false positive.
            auto o = Observation::parse(raw);
            dataset.push_back(o);
            auto d = evaluate(o, true, Inventory{100, 1, 1000, "init"}, Policy{}, 1000);
            blind.add(bad, true);
            verify.add(bad, false, true);
            confidence.add(bad, o.confidence >= 0.95);
            guard.add(bad, d.status == Status::Accept);
            if (examples.size() < 10)
                examples.push_back({{"observation", raw},
                                    {"ground_truth", 100},
                                    {"incorrect", bad},
                                    {"decision", d.json()}});
        }
        auto quality_seconds = std::chrono::duration<double>(Clock::now() - quality_start).count();
        Json policy_timings = Json::object();
        for (const auto &method : {"blind", "verify_everything", "confidence", "stockguard"}) {
            Integer accepts = 0;
            auto t = Clock::now();
            for (const auto &o : dataset) {
                bool accept = std::string_view(method) == "blind";
                if (std::string_view(method) == "confidence")
                    accept = o.confidence >= 0.95;
                if (std::string_view(method) == "stockguard")
                    accept =
                        evaluate(o, true, Inventory{100, 1, 1000, "init"}, Policy{}, 1000).status ==
                        Status::Accept;
                accepts += accept ? 1 : 0;
            }
            double seconds = std::chrono::duration<double>(Clock::now() - t).count();
            policy_timings[method] = {
                {"seconds", seconds},
                {"accepted", accepts},
                {"observations_per_second", static_cast<double>(n) / seconds}};
        }
        std::vector<double> latencies;
        latencies.reserve(static_cast<std::size_t>(n));
        Store s(path, {}, [] { return 1000; });
        seed(s);
        for (Integer w = 1; w < workers; ++w)
            s.execute({{"op", "initialize"},
                       {"product_id", "milk"},
                       {"location_id", "store" + std::to_string(w)},
                       {"quantity", 100},
                       {"actor", "seed"}});
        std::vector<std::unique_ptr<Store>> connections;
        for (Integer w = 0; w < workers; ++w)
            connections.push_back(std::make_unique<Store>(path, Policy{}, [] { return 1000; }));
        std::mutex results_mutex;
        std::exception_ptr failure;
        std::barrier gate(static_cast<std::ptrdiff_t>(workers + 1));
        std::vector<std::jthread> threads;
        for (Integer w = 0; w < workers; ++w)
            threads.emplace_back([&, w] {
                std::vector<double> local;
                gate.arrive_and_wait();
                try {
                    Integer version = 1;
                    for (Integer i = w; i < n; i += workers) {
                        auto t = Clock::now();
                        auto o = observation(i, 100, version++);
                        if (w)
                            o["location_id"] = "store" + std::to_string(w);
                        auto result = connections[static_cast<std::size_t>(w)]->execute(
                            {{"op", "ingest"}, {"observation", o}});
                        require(result["status"] == "ACCEPT", "benchmark_failure",
                                "Unexpected throughput decision");
                        local.push_back(
                            std::chrono::duration<double, std::micro>(Clock::now() - t).count());
                    }
                } catch (...) {
                    std::lock_guard lock(results_mutex);
                    failure = std::current_exception();
                }
                std::lock_guard lock(results_mutex);
                latencies.insert(latencies.end(), local.begin(), local.end());
            });
        auto start = Clock::now();
        gate.arrive_and_wait();
        threads.clear();
        if (failure)
            std::rethrow_exception(failure);
        auto elapsed = std::chrono::duration<double>(Clock::now() - start).count();
        std::sort(latencies.begin(), latencies.end());
        auto percentile = [&](double fraction) {
            return latencies.at(static_cast<std::size_t>(fraction * static_cast<double>(n - 1)));
        };
        Integer prevented = 0;
        for (Integer i = 0; i < 100; ++i) {
            Integer w = i % workers;
            auto o = observation(i, 100, i / workers + 1);
            if (w)
                o["location_id"] = "store" + std::to_string(w);
            auto replay = s.execute({{"op", "ingest"}, {"observation", o}});
            if (replay["replayed"] == true)
                ++prevented;
        }
        auto final = s.execute({{"op", "inventory"}});
        Integer total_versions = 0;
        for (const auto &row : final) {
            require(row["quantity"] == 100, "invariant_failure", "Final quantity invariant failed");
            total_versions += row["version"].get<Integer>() - 1;
        }
        require(total_versions == n, "invariant_failure", "Version invariant failed");
        rusage usage{};
        getrusage(RUSAGE_SELF, &usage);
        Json report = {{"seed", 2026},
                       {"observations", n},
                       {"compiler", __VERSION__},
                       {"hardware_threads", std::thread::hardware_concurrency()},
                       {"sqlite_version", sqlite3_libversion()},
                       {"policy", Policy{}.json()},
                       {"quality",
                        {{"blind", blind.json()},
                         {"verify_everything", verify.json()},
                         {"confidence", confidence.json()},
                         {"stockguard", guard.json()}}},
                       {"quality_seconds", quality_seconds},
                       {"policy_timings", policy_timings},
                       {"workers", workers},
                       {"quality_examples", examples},
                       {"throughput",
                        {{"seconds", elapsed},
                         {"observations_per_second", static_cast<double>(n) / elapsed},
                         {"median_us", percentile(0.5)},
                         {"p95_us", percentile(0.95)},
                         {"p99_us", percentile(0.99)},
                         {"max_rss_kib", usage.ru_maxrss},
                         {"duplicate_replays_prevented", prevented},
                         {"final_inventory", final},
                         {"total_applied_events", total_versions},
                         {"database_bytes", std::filesystem::file_size(path)}}}};
        std::cout << report.dump(2) << '\n';
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
