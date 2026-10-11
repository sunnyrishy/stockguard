#include "stockguard/store.hpp"
#include <fstream>
#include <iostream>
#include <memory>

int main(int argc, char **argv) {
    using namespace stockguard;
    if (argc < 3 || std::string(argv[1]) != "--db" || (argc != 3 && argc != 5)) {
        std::cerr << "Usage: stockguard --db PATH [--policy FILE]\n";
        return 2;
    }
    try {
        Policy policy;
        if (argc == 5) {
            require(std::string(argv[3]) == "--policy", "invalid_input", "Expected --policy");
            std::ifstream file(argv[4]);
            require(file.good(), "invalid_policy", "Cannot open policy file");
            Json p;
            file >> p;
            policy.minimum_confidence = p.at("minimum_confidence").get<double>();
            policy.relative_tolerance = p.at("relative_tolerance").get<double>();
            policy.absolute_tolerance = integer_field(p, "absolute_tolerance", 0, max_quantity);
            policy.max_age_seconds = integer_field(p, "max_age_seconds", 0, 86400);
            policy.future_skew_seconds = integer_field(p, "future_skew_seconds", 0, 300);
        }
        Store store(argv[2], policy);
        bool failed = false;
        // Read bounded lines before allocating a JSON tree. Oversized input is drained.
        while (std::cin.peek() != std::char_traits<char>::eof()) {
            std::string line;
            bool oversized = false;
            char c;
            while (std::cin.get(c) && c != '\n') {
                if (line.size() < 1'048'576)
                    line.push_back(c);
                else
                    oversized = true;
            }
            if (line.empty() && !oversized)
                continue;
            try {
                require(!oversized, "invalid_input", "Request exceeds 1 MiB");
                auto r = Json::parse(line, [](int depth, Json::parse_event_t, Json &) {
                    require(depth <= 64, "invalid_input", "JSON nesting exceeds 64 levels");
                    return true;
                });
                auto result = store.execute(r);
                std::cout << Json({{"ok", true}, {"result", result}}).dump() << '\n';
            } catch (const Error &e) {
                failed = true;
                std::cout << Json({{"ok", false}, {"error", e.code}, {"message", e.what()}}).dump()
                          << '\n';
            } catch (const Json::exception &e) {
                failed = true;
                std::cout << Json(
                                 {{"ok", false}, {"error", "invalid_input"}, {"message", e.what()}})
                                 .dump()
                          << '\n';
            }
            std::cout.flush();
        }
        return failed ? 1 : 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 2;
    }
}
