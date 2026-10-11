#include "stockguard/store.hpp"
#include <algorithm>

namespace stockguard {
Store::Store(const std::string &path, Policy policy, Clock clock, Checkpoint checkpoint)
    : database_(path), policy_(policy), clock_(std::move(clock)),
      checkpoint_(std::move(checkpoint)) {
    policy_.validate();
    migrate();
}
void Store::checkpoint(const std::string &name) {
    if (checkpoint_)
        checkpoint_(name);
}
void Store::migrate() {
    Transaction transaction(database_);
    auto version = database_.prepare("PRAGMA user_version");
    require(version.step(), "database", "Cannot read schema version");
    auto current = version.integer(0);
    require(current <= 1, "schema_version", "Database schema is newer than this application");
    if (current == 0)
        database_.exec(R"SQL(
CREATE TABLE products(
 id TEXT PRIMARY KEY, name TEXT NOT NULL, category TEXT NOT NULL, unit TEXT NOT NULL,
 status TEXT NOT NULL CHECK(status IN ('active','inactive')), metadata TEXT NOT NULL);
CREATE TABLE locations(id TEXT PRIMARY KEY);
CREATE TABLE inventory(
 product_id TEXT NOT NULL REFERENCES products(id), location_id TEXT NOT NULL REFERENCES locations(id),
 quantity INTEGER NOT NULL CHECK(quantity BETWEEN 0 AND 1000000000),
 version INTEGER NOT NULL CHECK(version > 0), updated_at INTEGER NOT NULL CHECK(updated_at >= 0),
 source TEXT NOT NULL, PRIMARY KEY(product_id,location_id));
CREATE TABLE observations(
 id TEXT PRIMARY KEY, scan_id TEXT NOT NULL, product_id TEXT NOT NULL, location_id TEXT NOT NULL,
 payload TEXT NOT NULL, decision TEXT NOT NULL,
 status TEXT NOT NULL CHECK(status IN ('ACCEPT','REVIEW','REJECT','DEFER')), created_at INTEGER NOT NULL);
CREATE TABLE scan_claims(
 scan_id TEXT NOT NULL, product_id TEXT NOT NULL, location_id TEXT NOT NULL,
 observation_id TEXT NOT NULL UNIQUE REFERENCES observations(id),
 PRIMARY KEY(scan_id,product_id,location_id));
CREATE TABLE review_actions(
 observation_id TEXT PRIMARY KEY REFERENCES observations(id), payload TEXT NOT NULL,
 result TEXT NOT NULL, actor TEXT NOT NULL, created_at INTEGER NOT NULL);
CREATE TABLE inventory_movements(
 id TEXT PRIMARY KEY, product_id TEXT NOT NULL REFERENCES products(id),
 location_id TEXT NOT NULL REFERENCES locations(id), delta INTEGER NOT NULL,
 payload TEXT NOT NULL, result TEXT NOT NULL, created_at INTEGER NOT NULL);
CREATE TABLE inventory_audit_log(
 id INTEGER PRIMARY KEY, product_id TEXT NOT NULL REFERENCES products(id),
 location_id TEXT NOT NULL REFERENCES locations(id), previous_quantity INTEGER NOT NULL CHECK(previous_quantity >= 0),
 new_quantity INTEGER NOT NULL CHECK(new_quantity BETWEEN 0 AND 1000000000),
 version INTEGER NOT NULL CHECK(version > 0), kind TEXT NOT NULL, event_id TEXT NOT NULL,
 actor TEXT NOT NULL, created_at INTEGER NOT NULL,
 UNIQUE(product_id,location_id,version));
CREATE INDEX observations_status_time ON observations(status,created_at,id);
CREATE INDEX audit_inventory ON inventory_audit_log(product_id,location_id,id);
PRAGMA user_version=1;
)SQL");
    transaction.commit();
}
bool Store::active_product(const std::string &product) {
    auto q = database_.prepare("SELECT status FROM products WHERE id=?");
    q.bind(1, product);
    return q.step() && q.text(0) == "active";
}
std::optional<Inventory> Store::inventory(const std::string &p, const std::string &l) {
    auto q = database_.prepare("SELECT quantity,version,updated_at,source FROM inventory WHERE "
                               "product_id=? AND location_id=?");
    q.bind(1, p);
    q.bind(2, l);
    if (!q.step())
        return std::nullopt;
    return Inventory{q.integer(0), q.integer(1), q.integer(2), q.text(3)};
}
void Store::audit(const std::string &p, const std::string &l, Integer previous, Integer next,
                  Integer version, const std::string &kind, const std::string &event,
                  const std::string &actor, Integer now) {
    auto q = database_.prepare(
        "INSERT INTO "
        "inventory_audit_log(product_id,location_id,previous_quantity,new_quantity,version,kind,"
        "event_id,actor,created_at) VALUES(?,?,?,?,?,?,?,?,?)");
    q.bind(1, p);
    q.bind(2, l);
    q.bind(3, previous);
    q.bind(4, next);
    q.bind(5, version);
    q.bind(6, kind);
    q.bind(7, event);
    q.bind(8, actor);
    q.bind(9, now);
    q.step();
}
void Store::update_inventory(const std::string &p, const std::string &l, const Inventory &previous,
                             Integer next, Integer timestamp, const std::string &source) {
    require(previous.version < 1'000'000'000'000LL, "version_limit", "Inventory version exhausted");
    auto q =
        database_.prepare("UPDATE inventory SET quantity=?,version=version+1,updated_at=?,source=? "
                          "WHERE product_id=? AND location_id=? AND version=?");
    q.bind(1, next);
    q.bind(2, timestamp);
    q.bind(3, source);
    q.bind(4, p);
    q.bind(5, l);
    q.bind(6, previous.version);
    q.step();
    require(database_.changes() == 1, "version_conflict", "Inventory changed concurrently");
    checkpoint("after_inventory");
}
Json Store::ingest(const Json &raw) {
    const auto o = Observation::parse(raw);
    auto existing = database_.prepare("SELECT payload,decision FROM observations WHERE id=?");
    existing.bind(1, o.id);
    if (existing.step()) {
        require(existing.text(0) == raw.dump(), "id_conflict",
                "Observation ID already has a different payload");
        auto result = Json::parse(existing.text(1));
        result["replayed"] = true;
        return result;
    }
    const auto now = clock_();
    auto state = inventory(o.product_id, o.location_id);
    auto decision = evaluate(o, active_product(o.product_id), state, policy_, now);
    auto scan = database_.prepare("SELECT observation_id FROM scan_claims WHERE scan_id=? AND "
                                  "product_id=? AND location_id=?");
    scan.bind(1, o.scan_id);
    scan.bind(2, o.product_id);
    scan.bind(3, o.location_id);
    const bool duplicate_scan = scan.step();
    if (duplicate_scan)
        decision = {Status::Reject,
                    "duplicate_scan",
                    "Scan key belongs to another observation",
                    {{"original_id", scan.text(0)}}};
    Json result = decision.json();
    result.update({{"id", o.id},
                   {"scan_id", o.scan_id},
                   {"product_id", o.product_id},
                   {"location_id", o.location_id},
                   {"decided_at", now},
                   {"replayed", false}});
    auto insert = database_.prepare("INSERT INTO observations VALUES(?,?,?,?,?,?,?,?)");
    insert.bind(1, o.id);
    insert.bind(2, o.scan_id);
    insert.bind(3, o.product_id);
    insert.bind(4, o.location_id);
    insert.bind(5, raw.dump());
    insert.bind(6, result.dump());
    insert.bind(7, to_string(decision.status));
    insert.bind(8, now);
    insert.step();
    checkpoint("after_observation");
    if (!duplicate_scan) {
        auto claim = database_.prepare("INSERT INTO scan_claims VALUES(?,?,?,?)");
        claim.bind(1, o.scan_id);
        claim.bind(2, o.product_id);
        claim.bind(3, o.location_id);
        claim.bind(4, o.id);
        claim.step();
    }
    if (decision.status == Status::Accept) {
        update_inventory(o.product_id, o.location_id, *state, o.quantity,
                         std::max(now, o.observed_at), "observation");
        audit(o.product_id, o.location_id, state->quantity, o.quantity, state->version + 1,
              "observation", o.id, o.source, now);
    }
    return result;
}
Json Store::review(const Json &r) {
    const auto id = identifier(r, "observation_id");
    const auto actor = identifier(r, "actor");
    const auto action = text_field(r, "action");
    require(action == "confirm" || action == "correct" || action == "reject", "invalid_input",
            "Unsupported review action");
    auto prior =
        database_.prepare("SELECT payload,result FROM review_actions WHERE observation_id=?");
    prior.bind(1, id);
    if (prior.step()) {
        require(prior.text(0) == r.dump(), "review_conflict",
                "Observation already resolved with another action");
        auto result = Json::parse(prior.text(1));
        result["replayed"] = true;
        return result;
    }
    auto query = database_.prepare("SELECT payload,decision,status FROM observations WHERE id=?");
    query.bind(1, id);
    require(query.step(), "not_found", "Observation does not exist");
    require(query.text(2) == "REVIEW" || query.text(2) == "DEFER", "not_reviewable",
            "Observation is not pending review or deferred");
    const auto o = Observation::parse(Json::parse(query.text(0)));
    require(active_product(o.product_id), "invalid_product", "Product is missing or inactive");
    const auto state = inventory(o.product_id, o.location_id);
    require(state.has_value(), "missing_inventory", "Initialize trusted inventory before review");
    auto expected = integer_field(r, "expected_version", 1, 1'000'000'000'000LL);
    require(expected == state->version, "version_conflict",
            "Inventory changed, perform fresh verification");
    const auto now = clock_();
    Integer next = state->quantity;
    if (action != "reject") {
        auto verified = integer_field(r, "verified_at", 0, 253402300799LL);
        require(verified >= state->updated_at && verified <= now + policy_.future_skew_seconds &&
                    now - verified <= policy_.max_age_seconds,
                "stale_verification", "Fresh complete human verification is required");
        next = action == "correct" ? integer_field(r, "quantity", 0, max_quantity) : o.quantity;
        update_inventory(o.product_id, o.location_id, *state, next, std::max(now, verified),
                         "human_review");
        audit(o.product_id, o.location_id, state->quantity, next, state->version + 1, "review", id,
              actor, now);
    }
    Json result = {{"observation_id", id},
                   {"action", action},
                   {"actor", actor},
                   {"quantity", next},
                   {"version", state->version + (action == "reject" ? 0 : 1)},
                   {"resolved_at", now},
                   {"replayed", false}};
    auto insert = database_.prepare("INSERT INTO review_actions VALUES(?,?,?,?,?)");
    insert.bind(1, id);
    insert.bind(2, r.dump());
    insert.bind(3, result.dump());
    insert.bind(4, actor);
    insert.bind(5, now);
    insert.step();
    return result;
}
Json Store::movement(const Json &r) {
    auto id = identifier(r, "id");
    auto p = identifier(r, "product_id");
    auto l = identifier(r, "location_id");
    auto actor = identifier(r, "actor");
    auto delta = integer_field(r, "delta", -max_quantity, max_quantity);
    auto expected = integer_field(r, "expected_version", 1, 1'000'000'000'000LL);
    auto prior = database_.prepare("SELECT payload,result FROM inventory_movements WHERE id=?");
    prior.bind(1, id);
    if (prior.step()) {
        require(prior.text(0) == r.dump(), "id_conflict", "Movement ID has a different payload");
        auto result = Json::parse(prior.text(1));
        result["replayed"] = true;
        return result;
    }
    require(active_product(p), "invalid_product", "Product is missing or inactive");
    auto state = inventory(p, l);
    require(state.has_value(), "missing_inventory", "Inventory does not exist");
    require(state->version == expected, "version_conflict", "Inventory version changed");
    auto next = state->quantity + delta;
    require(next >= 0 && next <= max_quantity, "invalid_quantity",
            "Movement would violate quantity bounds");
    auto now = clock_();
    require(now >= state->updated_at, "clock_regression",
            "Server time predates last inventory update");
    update_inventory(p, l, *state, next, now, "movement");
    audit(p, l, state->quantity, next, state->version + 1, "movement", id, actor, now);
    Json result = {
        {"id", id}, {"quantity", next}, {"version", state->version + 1}, {"replayed", false}};
    auto insert = database_.prepare("INSERT INTO inventory_movements VALUES(?,?,?,?,?,?,?)");
    insert.bind(1, id);
    insert.bind(2, p);
    insert.bind(3, l);
    insert.bind(4, delta);
    insert.bind(5, r.dump());
    insert.bind(6, result.dump());
    insert.bind(7, now);
    insert.step();
    return result;
}
Json Store::execute(const Json &r) {
    const auto op = text_field(r, "op");
    if (op == "batch") {
        require(r.contains("observations") && r.at("observations").is_array() &&
                    !r.at("observations").empty() && r.at("observations").size() <= 1000,
                "invalid_input", "Batch requires 1 to 1000 observations");
        const auto batch_id = identifier(r, "batch_id");
        Json results = Json::array();
        for (const auto &item : r.at("observations")) {
            try {
                results.push_back(
                    {{"ok", true}, {"result", execute({{"op", "ingest"}, {"observation", item}})}});
            } catch (const Error &e) {
                results.push_back({{"ok", false}, {"error", e.code}, {"message", e.what()}});
            } catch (const Json::exception &e) {
                results.push_back(
                    {{"ok", false}, {"error", "invalid_input"}, {"message", e.what()}});
            }
        }
        return {{"batch_id", batch_id}, {"consistency", "per_observation"}, {"results", results}};
    }
    std::lock_guard guard(mutex_);
    Transaction transaction(database_);
    auto result = execute_locked(r);
    checkpoint("before_commit");
    transaction.commit();
    return result;
}
Json Store::execute_locked(const Json &r) {
    const auto op = text_field(r, "op");
    if (op == "ingest") {
        require(r.contains("observation"), "invalid_input", "Missing observation");
        return ingest(r.at("observation"));
    }
    if (op == "review")
        return review(r);
    if (op == "movement")
        return movement(r);
    if (op == "product_put") {
        auto id = identifier(r, "id");
        auto name = text_field(r, "name", 256);
        auto category = text_field(r, "category");
        auto unit = text_field(r, "unit");
        auto status = text_field(r, "status");
        require(status == "active" || status == "inactive", "invalid_input",
                "Invalid product status");
        Json metadata = r.value("metadata", Json::object());
        require(metadata.is_object() && metadata.dump().size() <= 4096, "invalid_input",
                "Invalid metadata");
        auto previous = database_.prepare("SELECT unit FROM products WHERE id=?");
        previous.bind(1, id);
        if (previous.step() && previous.text(0) != unit) {
            auto used = database_.prepare("SELECT 1 FROM inventory WHERE product_id=? LIMIT 1");
            used.bind(1, id);
            require(!used.step(), "unit_change",
                    "Cannot change units while trusted inventory exists");
        }
        auto q = database_.prepare(
            "INSERT INTO products VALUES(?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET "
            "name=excluded.name,category=excluded.category,unit=excluded.unit,status=excluded."
            "status,metadata=excluded.metadata");
        q.bind(1, id);
        q.bind(2, name);
        q.bind(3, category);
        q.bind(4, unit);
        q.bind(5, status);
        q.bind(6, metadata.dump());
        q.step();
        return {{"id", id}, {"status", status}};
    }
    if (op == "initialize") {
        auto p = identifier(r, "product_id");
        auto l = identifier(r, "location_id");
        auto qty = integer_field(r, "quantity", 0, max_quantity);
        auto actor = identifier(r, "actor");
        require(active_product(p), "invalid_product", "Product must be active");
        require(!inventory(p, l).has_value(), "already_exists", "Inventory already initialized");
        auto location = database_.prepare("INSERT OR IGNORE INTO locations VALUES(?)");
        location.bind(1, l);
        location.step();
        auto now = clock_();
        auto q = database_.prepare("INSERT INTO inventory VALUES(?,?,?,1,?,?)");
        q.bind(1, p);
        q.bind(2, l);
        q.bind(3, qty);
        q.bind(4, now);
        q.bind(5, "initialization");
        q.step();
        checkpoint("after_inventory");
        audit(p, l, 0, qty, 1, "initialization", p + ":" + l, actor, now);
        return {{"quantity", qty}, {"version", 1}};
    }
    if (op == "inventory") {
        Json result = Json::array();
        auto q =
            database_.prepare("SELECT product_id,location_id,quantity,version,updated_at,source "
                              "FROM inventory ORDER BY product_id,location_id LIMIT ? OFFSET ?");
        q.bind(1, r.contains("limit") ? integer_field(r, "limit", 1, 1000) : 1000);
        q.bind(2, r.contains("offset") ? integer_field(r, "offset", 0, 1'000'000'000) : 0);
        while (q.step())
            result.push_back({{"product_id", q.text(0)},
                              {"location_id", q.text(1)},
                              {"quantity", q.integer(2)},
                              {"version", q.integer(3)},
                              {"updated_at", q.integer(4)},
                              {"source", q.text(5)}});
        return result;
    }
    if (op == "products") {
        Json result = Json::array();
        auto q = database_.prepare("SELECT id,name,category,unit,status,metadata FROM products "
                                   "WHERE (?='' OR id=?) ORDER BY id LIMIT ? OFFSET ?");
        auto id = r.contains("id") ? identifier(r, "id") : "";
        q.bind(1, id);
        q.bind(2, id);
        q.bind(3, r.contains("limit") ? integer_field(r, "limit", 1, 1000) : 1000);
        q.bind(4, r.contains("offset") ? integer_field(r, "offset", 0, 1'000'000'000) : 0);
        while (q.step())
            result.push_back({{"id", q.text(0)},
                              {"name", q.text(1)},
                              {"category", q.text(2)},
                              {"unit", q.text(3)},
                              {"status", q.text(4)},
                              {"metadata", Json::parse(q.text(5))}});
        return result;
    }
    if (op == "history" || op == "pending") {
        auto q = database_.prepare(
            op == "pending"
                ? "SELECT decision FROM observations o WHERE status IN ('REVIEW','DEFER') AND NOT "
                  "EXISTS(SELECT 1 FROM review_actions r WHERE r.observation_id=o.id) ORDER BY "
                  "created_at,id LIMIT ? OFFSET ?"
                : "SELECT decision FROM observations ORDER BY created_at,id LIMIT ? OFFSET ?");
        q.bind(1, r.contains("limit") ? integer_field(r, "limit", 1, 1000) : 1000);
        q.bind(2, r.contains("offset") ? integer_field(r, "offset", 0, 1'000'000'000) : 0);
        Json result = Json::array();
        while (q.step())
            result.push_back(Json::parse(q.text(0)));
        return result;
    }
    if (op == "reviews") {
        auto q = database_.prepare("SELECT result FROM review_actions ORDER BY "
                                   "created_at,observation_id LIMIT ? OFFSET ?");
        q.bind(1, r.contains("limit") ? integer_field(r, "limit", 1, 1000) : 1000);
        q.bind(2, r.contains("offset") ? integer_field(r, "offset", 0, 1'000'000'000) : 0);
        Json result = Json::array();
        while (q.step())
            result.push_back(Json::parse(q.text(0)));
        return result;
    }
    if (op == "audit") {
        auto q = database_.prepare(
            "SELECT "
            "id,product_id,location_id,previous_quantity,new_quantity,version,kind,event_id,actor,"
            "created_at FROM inventory_audit_log ORDER BY id LIMIT ? OFFSET ?");
        q.bind(1, r.contains("limit") ? integer_field(r, "limit", 1, 1000) : 1000);
        q.bind(2, r.contains("offset") ? integer_field(r, "offset", 0, 1'000'000'000) : 0);
        Json result = Json::array();
        while (q.step())
            result.push_back({{"id", q.integer(0)},
                              {"product_id", q.text(1)},
                              {"location_id", q.text(2)},
                              {"previous_quantity", q.integer(3)},
                              {"new_quantity", q.integer(4)},
                              {"version", q.integer(5)},
                              {"kind", q.text(6)},
                              {"event_id", q.text(7)},
                              {"actor", q.text(8)},
                              {"created_at", q.integer(9)}});
        return result;
    }
    if (op == "metrics") {
        Json counts = Json::object(), reasons = Json::object();
        Integer total = 0;
        auto q = database_.prepare("SELECT status,count(*) FROM observations GROUP BY status");
        while (q.step()) {
            counts[q.text(0)] = q.integer(1);
            total += q.integer(1);
        }
        auto reason = database_.prepare(
            "SELECT json_extract(decision,'$.reason'),count(*) FROM observations GROUP BY 1");
        while (reason.step())
            reasons[reason.text(0)] = reason.integer(1);
        return {
            {"observations", total},
            {"status_counts", counts},
            {"reason_counts", reasons},
            {"acceptance_rate",
             total ? counts.value("ACCEPT", 0.0) / static_cast<double>(total) : 0.0},
            {"review_rate", total ? counts.value("REVIEW", 0.0) / static_cast<double>(total) : 0.0},
            {"policy", policy_.json()}};
    }
    throw Error("invalid_operation", "Unknown command");
}
} // namespace stockguard
