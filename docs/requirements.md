# Requirements and acceptance criteria
| Requirement | Acceptance check |
| --- | --- |
| Catalog | Create, update, retrieve and list products, enforce valid IDs and status |
| Trusted state | Nonnegative bounded quantity, monotonic version and timestamp |
| Input | Typed fields, bounded strings and quantities, finite confidence, valid timestamps |
| Decisions | ACCEPT, REVIEW, REJECT, DEFER with persisted reasons and evidence |
| Evidence | Version, age, coverage, confidence and discrepancy all checked |
| Replay | Durable same-ID replay returns original result; changed payload conflicts |
| Review | Confirm, correct or reject, one terminal action per event, version conflict safe |
| Audit | Every quantity/version change and audit insertion share a transaction |
| Movement | Signed bounded deltas, durable replay, optimistic version checking |
| Batch | Per-item commits, bounded input and explicit partial success |
| Reporting | Inventory, decisions, reviews, audit and status/reason counts |
| Concurrency | Separate connections serialize writes with BEGIN IMMEDIATE and bounded busy wait |
| Recovery | WAL, foreign keys, FULL synchronous, rollback and restart tests |
| Demo | Loopback HTTP UI drives the C++ executable, representative functional smoke test |
| Measurement | Seeded synthetic correctness comparison and separately timed throughput |

Required gates: warning-clean C++20 build, domain tests, persistence and rollback tests,
concurrent replay and review tests, functional demo smoke test and reproducible benchmarks.
ASan/UBSan and formatting required where supported. TSan and clang-tidy availability
and runtime limitations are reported. No zero-test runs count as validation.
