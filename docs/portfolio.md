# Publication preparation
Status: local repository and publication materials prepared. Nothing pushed or published.
Repository visibility changes and LinkedIn publication require the owner's final approval.
Proposed title: StockGuard: Fault-Tolerant Inventory Reconciliation in C++20.
Proposed description: Explainable inventory count validation with transactional SQLite,
durable idempotency, optimistic human review, concurrency tests and synthetic benchmarks.
Suggested topics: cpp20, sqlite, inventory, reconciliation, reliability, concurrency, cmake.
MIT license selected for the independent project. Review license choice before publication.

## LinkedIn draft
I built StockGuard to explore a backend reliability problem: how should a system handle a
confident inventory prediction that conflicts with its trusted ledger?

The C++20 engine separates scanner observations from authoritative inventory. It checks
coverage, freshness, ledger version, confidence and discrepancy, then explains whether a
count is accepted, reviewed, rejected or deferred. SQLite commits trusted updates and audit
records together. Durable IDs make replays safe, and human corrections check record versions.

The local implementation has 30 automated tests covering policy boundaries, rollback,
restart recovery, concurrent duplicate submissions and conflicting reviews. I also exercised
the browser workflow and built reproducible synthetic comparisons.

In a labeled 10,000-snapshot experiment, the combined policy gated 4,031 of 5,070 incorrect
counts, compared with 1,064 for a simple confidence threshold. It still accepted 1,039
plausible wrong counts. That limit matters: database consistency cannot prove a model's
prediction is true, and selective verification trades review workload against missed errors.

The most useful lessons were explicit transaction boundaries, precise count semantics, and
measuring the cost of explainability. The project is independent, uses synthetic data, and
is not deployed in a store. Full methodology and limits are in the repository.

[Add public repository link only after approved publication.]

## Resume bullets
- Implemented a C++20 inventory reconciliation engine with explainable policy decisions,
  SQLite transactions, durable replay protection and version-checked human corrections.
- Verified inventory/audit consistency through 30 automated tests, including concurrent
  submissions, transaction fault injection and process-restart recovery; ran ASan/UBSan
  and scoped TSan analysis with a documented upstream SQLite annotation limit.
- Compared four policies on 10,000 labeled synthetic snapshots, gating 4,031 of 5,070
  incorrect counts while reporting 1,039 false negatives and 991 correct counts gated.

## 30-second explanation
StockGuard is a C++20 reliability layer for uncertain inventory counts. A scan supplies a
complete count and the ledger version it observed. The engine checks whether that count
is fresh, complete and consistent with recorded movements. Accepted changes and audits
commit together in SQLite. Duplicates replay safely, while uncertain counts require a fresh
human verification. The tests exercise concurrency and rollback, and the benchmarks expose
both performance and detection limits.

## Two-minute technical explanation
Inventory facts and scanner observations are different kinds of data. StockGuard retains
both, with immutable original decisions. A movement updates the trusted ledger and increments
its version. A scanner count using an older version cannot overwrite it. Fresh complete
counts pass through configurable confidence and discrepancy gates. Uncertain counts leave
inventory unchanged and carry enough evidence for an operator to understand the reason.

The C++ library has a pure policy function and a transactional application service. SQLite
BEGIN IMMEDIATE makes read-decide-write one serialized operation, while unique observation
and scan keys protect against replays, including across process restarts. RAII rolls back
exceptions. Human review is a separate transaction with a current version check and a fresh
verification timestamp. The audit ledger records every trusted version change.

A JSON-lines executable is the only integration boundary. The Python browser adapter runs
that executable with bounded requests and timeouts. This is deliberately a local demonstration
rather than an authenticated production platform. Tests include simultaneous duplicates,
conflicting reviewers and process death before commit. Synthetic evaluation compares blind
acceptance, verify-everything, confidence-only and combined policy. The combined policy still
misses plausible wrong counts; its correctness guarantee is transactional consistency, not
physical truth. SQLite is one writer, so measured concurrency does not justify a worker pool.

## Architecture deep dive
Start with docs/architecture.md, then trace Store::ingest: validated payload, durable ID lookup,
scan claim, versioned inventory read, pure evaluation, observation/decision insertion, optional
inventory/audit update, commit. Explain how an exception unwinds statements before the RAII
transaction rolls back. Next trace Store::review and the preserved original decision. Finish
with the query snapshot and single-writer tradeoffs, then ADR 002 for dependency research.

## Interview questions
1. Why is confidence not sufficient? It is uncalibrated and does not establish coverage,
   freshness, product correctness or consistency with movements. Small confident errors remain.
2. What if the client loses a response? It retries the same ID and payload. The committed
   decision replays; an uncommitted write rolls back. Changed payloads conflict.
3. How do you avoid inventory without an audit? Both writes occur in one transaction, with
   injected failures tested between the update and audit and before commit.
4. Does WAL give concurrent writers? No. SQLite allows one writer. BEGIN IMMEDIATE acquires
   that transaction early, and the bounded busy timeout can fail explicitly.
5. Is an ACCEPT decision proof of true inventory? No. It proves only that supplied evidence
   satisfied the selected policy and that state and audit were committed consistently.

## GitHub profile summary
StockGuard is an independent C++20 inventory reconciliation project with explainable rules,
transactional SQLite state, replay protection, human review, concurrency tests and openly
reported synthetic false negatives. Includes a local browser demonstration and measured reports.
