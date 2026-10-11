# Limits and next work
- This is a functional local MVP, not a deployed inventory service or a vision model.
- Full product-location integer counts only. No shelf-region merge, fractional units,
  scanner calibration, model substitutions or unrecorded movement inference.
- Default discrepancy tolerances can accept plausible wrong counts. The benchmark reports
  those false negatives explicitly. Synthetic detection rates do not imply real-store accuracy.
- Human verification is modeled as fresh complete observation; the software cannot prove
  a person actually counted correctly. Actor strings are synthetic attribution. Production
  authentication, authorization and remote transport security are not implemented.
- SQLite is one writer. BEGIN IMMEDIATE is also used for queries, which limits read concurrency.
  WAL requires a suitable local filesystem, not an arbitrary network share.
- Audit is durable and transactionally consistent, not tamper-proof against database-file owners.
  No replication, backup automation, disaster recovery SLA or physically tested power-loss recovery.
- Batch envelope IDs are not persisted; item and scan IDs provide idempotency. Batches partially
  commit. Query pages do not form a stable snapshot across concurrent changes.
- The default schema is versioned, with an initial migration only. There is no downgrade path,
  retention policy or exported backup command.
- The benchmark uses independent labeled snapshots for quality, and consistent count events
  for durable throughput. FULL synchronous on tmpfs measures transaction machinery, not physical
  disk flush latency. Initial tmpfs figures are one run per configuration. The separate workspace experiment
  has three repetitions, still insufficient for a statistical capacity estimate.
- Largest measured workload is 100,000 observations. A million-event run is supported but unrun.
  Confidence calibration, real labeled data and direct lock-wait profiling remain future work.
- GCC TSan applies one documented upstream SQLite WAL-header suppression. Application code
  and other SQLite paths remain instrumented; this is an explicit analysis boundary.
- Reuters' specific May 21, 2026 report remains unverified due to an access challenge. Official
  Starbucks announcements retrieved do not establish that discontinuation claim.
- GitHub CI, publication, fresh cloud-task restoration and Windows/WSL execution are unverified.

Next priorities: verify the news source, obtain authorized realistic labels, calibrate policy,
add authenticated service boundaries and backup tooling, then measure workloads on target
persistent storage before considering a queue, statement cache or changed transaction model.
