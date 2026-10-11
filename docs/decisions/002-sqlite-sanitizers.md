# ADR 002: Patched SQLite and scoped TSan annotation compatibility
The original system SQLite 3.46.1 falls in the official WAL-reset bug's affected range.
The application now compiles checksum-verified upstream SQLite 3.54.0 by default.
SQLite is instrumented alongside the application in sanitizer builds.

With GCC 14 TSan and fully instrumented SQLite, three multi-connection tests initially
returned nonzero due to WAL-header dirty-read reports. Their functional assertions passed.
The shared-Store test had no report. Inspecting every report identified walIndexWriteHdr
on the write side and walIndexTryHdr or walTryBeginRead on the read side. These are header
copies in memory-mapped shared storage, checked by duplicate reads, checksums and barriers.

Upstream SQLite sqlite3.c lines 67806 to 67834 and 69505 to 69548 explicitly describe this
protocol, identify TSan false positives with tag-20200519-1, and mark walIndexWriteHdr and
walIndexTryHdr with SQLITE_NO_TSAN. In this release that annotation is enabled for Clang,
not GCC. We preserve the downloaded source and enable one runtime suppression,
`race:walIndexWriteHdr`, matching the documented upstream write function. The suppression
also covers its corresponding read-side reports. No application function is suppressed;
all assertions, ledger invariants and the rest of SQLite instrumentation remain enabled.

The suppression is committed in config/tsan.supp and applied by CTest only in TSan mode.
It may hide future defects that involve the same upstream function, so this is a disclosed
instrumentation limit, not proof that SQLite has no races. Do not extend it to broad SQLite
or StockGuard patterns. Review it when upgrading SQLite or changing compilers.

References: https://sqlite.org/wal.html (WAL-reset fix), and the official checksum-pinned
3.54.0 amalgamation. The initial unsuppressed report remains in the ignored runtime log.
