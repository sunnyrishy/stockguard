# Concurrency
One Store instance is protected by a mutex. Each public operation holds that mutex and uses
BEGIN IMMEDIATE. Independent Store instances and CLI processes coordinate through SQLite
writer locking, a five-second busy timeout, WAL and version checks. Busy expiry is surfaced
as an error, never interpreted as a successful commit. Reads share the transaction wrapper
for simple consistent snapshots, at the cost of unnecessary writer contention.

Unique event and scan keys live in the database, not a volatile cache. Simultaneous review
and ingestion compete through the writer lock and version predicate. Threads are joined
before test resources are destroyed. No asynchronous queue is added without throughput
measurement justifying its lifecycle and shutdown complexity.
