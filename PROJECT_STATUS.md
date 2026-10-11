# StockGuard project status

Updated October 11, 2026. Current phase: local MVP handoff and publication preparation.
The selected development workflow works in this cloud instance. The project is published to its existing GitHub repository.
Fresh-task restoration is not verified. The specific Reuters motivation remains unverified.

## Completed and verified
- [x] Inspected the initially empty existing repository and verified GitHub read access.
- [x] Verified C++20 builds with GCC 14.2 and installed repeatable user-local tooling.
- [x] Recorded requirements, scope, data semantics, source boundaries and architecture diagrams.
- [x] Implemented catalog, trusted inventory, observations, policy decisions and movement ledger.
- [x] Persisted decisions, trusted updates and audit with SQLite transactions and prepared statements.
- [x] Implemented ID/scan replay protection, conflicts, partial batches and version-checked review.
- [x] Added queries, pending review, metrics, immutable history and audit/review reporting.
- [x] Implemented bounded local HTTP adapter and browser scenario/review demonstration.
- [x] Passed 33 GoogleTests in Debug, Release and ASan/UBSan builds.
- [x] Passed four concurrency tests under GCC TSan with one documented upstream WAL suppression.
- [x] Passed clang-format 19 checks and selected clang-tidy analyzers with no project diagnostics.
- [x] Passed HTTP functional smoke, JSON request bounds and cross-site request rejection checks.
- [x] Passed a real Chromium browser workflow and captured docs/images/dashboard.png.
- [x] Ran seeded 1,000/10,000/100,000-event benchmarks and 1/2/4-worker comparisons.
- [x] Compared four policies against synthetic labels and explicitly reported false negatives.
- [x] Ran a separate GCC -pg profile; retained a flat-profile excerpt.
- [x] Exercised the repeatable cloud bootstrap, README example and idempotent service starter.
- [x] Prepared CI configuration, MIT license, dependency notices and publication/career drafts.
- [x] Saved cloud install_script and start_skill, plus additive research domain settings.
- [x] Created a local implementation commit on main; the owner-authorized remote push is now completed.

## Decisions
- Complete nonnegative integer product-location counts; no mixed movement/count semantics.
- Recorded movements update expected state; version and freshness checks prevent stale overwrites.
- C++20 modular monolith, pure rule function, JSON-lines executable and Python standard-library demo.
- SQLite WAL, foreign keys, synchronous=FULL, BEGIN IMMEDIATE and a five-second busy timeout.
- Pin and compile SQLite 3.54.0 after official research identified an older WAL-reset defect.
- Keep initial decisions immutable; human resolutions are separate transactional records.
- No asynchronous ingestion queue, based on the single-writer model and measured scaling.
- Apply one named upstream WAL-header TSan suppression for GCC, without suppressing StockGuard.

## Checks and evidence
Detailed commands: docs/testing.md. Raw counters and measurements: reports/benchmark-*.json.
Final suite: 33 passed, zero failed/skipped in Debug, Release and ASan/UBSan. Scoped TSan: 4 passed.
HTTP smoke, browser smoke, formatting, selected static analysis and README example passed.
SQLite is 3.54.0 in final builds. The original unsuppressed GCC TSan run returned nonzero
on SQLite's documented dirty-read protocol; diagnosis and limit are in ADR 002.
Hosted CI was triggered by the first push; results are tracked on GitHub Actions.
WSL2 and fresh cloud-task restoration have not been tested.

## Benchmark status
Final reports measure SQLite 3.54.0. Largest workload: 100,000 events. No million-event run.
Initial throughput runs use /tmp tmpfs. Follow-up runs use repository workspace overlayfs.
Both include transaction/audit work and neither proves physical disk durability.
The 10,000-snapshot quality run gates 4,031/5,070 incorrect counts but accepts 1,039 plausible
wrong counts and gates 991 correct counts. This is synthetic evidence, not production accuracy.
No measured performance optimization is claimed. Further work needs physical storage validation, more repetitions, real authorized labels and a calibrated
verification cost model. Three 10,000-event workspace-storage runs now exist, with independent
read-only database integrity and audit-total checks. See reports/persistent and docs/benchmarks.md.

## Known issues and boundaries
No known critical StockGuard data-corruption defect was found by the executed tests. This
is not a proof of absence. There is no production authentication, backup automation,
replication, tamper-proof audit or physical power-loss experiment. See docs/limitations.md.
The claimed Reuters report could not be read due to a 401 access challenge. Official
Starbucks material accessed does not corroborate the specific discontinued-counting claim.

## Remaining tasks and required next actions
- [ ] Verify the exact Reuters article and additional research before using a corporate case study.
- [x] Owner authorized publishing the current project to its existing GitHub repository.
- [x] Triggered hosted CI through an owner-authorized push.
- [ ] Confirm the WSL build on the owner's machine.
- [ ] Review saved cloud environment settings and publish the prepared environment through the UI.
- [ ] Validate a new cloud task independently; do not infer restoration from current-instance success.
- [ ] Add authenticated service boundaries and backups before any remote operational deployment.

GitHub publication was explicitly authorized and completed. LinkedIn content remains a draft.
No visibility change or third-party message has been sent. Resume claims are constrained to implemented code and actual measurements.

## Cloud configuration handoff
The draft save returned status=saved. install_script contains the tested cloud bootstrap,
formatting, static analysis and functional HTTP check. start_skill records tool activation,
working directory, build/test commands, service startup, readiness and process-restart rules.
Research domains were added while preserving package-manager presets. Saving did not publish
the environment. No credentials were required. The implementation is now pushed to GitHub. A new cloud task still needs independent
validation; saving configuration alone does not verify restoration.

## Follow-up verification
- [x] Enhanced the runner with explicit storage selection, repetition counts, binary provenance,
  atomic report writes and grouped minimum/median/maximum summaries.
- [x] Completed three 10,000-event workspace-storage runs on overlayfs with closed-database
  integrity, foreign-key, inventory, observation and audit-count verification.
- [x] Passed two regression checks preventing false successful benchmark reports and one
  real 100-event reporting-path smoke run. No C++ implementation changed in this follow-up.
- [x] Rechecked Reuters search, which still returned an HTTP 401 access challenge.
- [ ] Bing and Yahoo research routes returned HTTP 403; their domains were added to the
  environment draft without changing existing network entries. News verification remains open.

The follow-up cloud draft retains existing setup/start instructions and adds the two Python
reporting guards. The draft save was confirmed. Cloud configuration publication remains unperformed.

## Review resolution fix
Pending evidence can now be rejected after catalog deactivation, without an inventory record,
or after the ledger version advances. Confirmation and correction retain their existing checks.
Three regression tests cover closure, durable replay, unchanged audit totals and conflicting actors.
The HTTP check and real Chromium workflow verify rejection without a supplied version.
All 33 tests pass in Debug, Release and ASan/UBSan; scoped TSan, formatting and static analysis pass.

## GitHub publication
The owner authorized pushing the current implementation and requested README flowcharts.
The README now includes system architecture, decision flow and concurrent-version behavior.
The first push was verified against the remote main commit. The GitHub repository page
contains the new diagrams, and Actions run 38161482448 started automatically.
Repository visibility was not changed. Cloud configuration publication remains pending.

## Fresh-clone verification
Cloned GitHub commit 72712bd into a new temporary directory with no existing build files.
The README configure/build steps, 33 C++ tests, HTTP smoke and two Python reporting guards passed.
The demo starter passed health checks and reused its process on a second invocation.
The temporary demo process was stopped after verification. This verifies Git-based setup
on this Linux host, not a new cloud task or the owner's WSL installation.
Latest hosted CI for that commit passed: Actions run 38162276395.
