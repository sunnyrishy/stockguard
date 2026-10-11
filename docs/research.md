# Source log and evidence boundaries
Research accessed October 11, 2026. Public web access was initially restricted. Research domains were added to the saved
environment draft. Later retries successfully retrieved SQLite and Starbucks official
content; Reuters returned HTTP 401 with an access challenge, which was not bypassed. No Starbucks operational facts are asserted in the
project's problem statement beyond the user's unverified motivation.

| Source | Publication/version | URL | Result and relevance |
| --- | --- | --- | --- |
| Reuters report mentioned by owner | Claimed May 21, 2026 | https://www.reuters.com/business/retail-consumer/ | Initial proxy denial, later HTTP 401 access challenge; exact article and claims unverified |
| Starbucks official newsroom | Dynamic, no specific article verified | https://about.starbucks.com/ | Retrieved homepage and public WordPress search; no discontinued-counting claim found |
| SQLite transaction documentation | Updated February 18, 2026 | https://sqlite.org/lang_transaction.html | Retrieved and read; BEGIN IMMEDIATE acquires write transaction, commit/rollback semantics |
| SQLite WAL documentation | Updated August 25, 2026 | https://sqlite.org/wal.html | Retrieved and read; single writer, FULL synchronous, WAL-reset corruption bug fixed in 3.51.3+ |
| SQLite download page | 3.54.0 observed October 11, 2026 | https://sqlite.org/download.html | Official amalgamation SHA3 verified, SHA256 pinned in CMake; replaces older affected library |
| Starbucks Leadership Experience | June 10, 2025 | https://about.starbucks.com/stories/2025/top-moments-from-starbucks-leadership-experience-2025-in-las-vegas/ | Official announcement describes a virtual assistant with inventory guidance, not evidence of the alleged failed counting system |
| Starbucks operations overview | September 8, 2025 | https://about.starbucks.com/stories/2025/how-back-to-starbucks-is-reshaping-every-aspect-of-the-coffeehouse-experience/ | Retrieved and read; operational context, no confirmation of the counting claim |
| SQLite official source | 3.46.1 | https://github.com/sqlite/sqlite/blob/version-3.46.1/src/main.c | Successfully retrieved official source; connection and transaction implementation reference |
| SQLite WAL source | 3.46.1 | https://github.com/sqlite/sqlite/blob/version-3.46.1/src/wal.c | Successfully retrieved, comments describe commit and recovery behavior |
| C++ Core Guidelines | HEAD observed as 33bcd015997f0d8e0fa0202eb66254a16f59ad8f | https://github.com/isocpp/CppCoreGuidelines | Successfully retrieved; RAII, ownership and synchronization guidance |
| GoogleTest | v1.16.0 | https://github.com/google/googletest/tree/6910c9d9165801d8827d628cb72eb7ea9dd538c5 | Retrieved and executed; unit and integration test framework |
| nlohmann JSON | v3.11.3 | https://github.com/nlohmann/json/tree/9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03 | Retrieved, used for bounded command parsing and deterministic serialization |

The engineering problem is valid independently of the news claim: observations are not
trusted inventory facts. Complete coverage, confidence, freshness and ledger version are
separate signals. The implementation uses deterministic policy checks and human verification,
not a statistically validated detector. It includes a deliberately undetectable plausible
error scenario. No model confidence calibration experiment is claimed because there are
no real classifier predictions or authorized real inventory labels.

Remaining research: verify the exact Reuters article and date, inspect official Starbucks
announcements, and review peer-reviewed calibration and inventory reconciliation literature
before framing a public case study around a specific corporate incident. The prepared
case study uses only the independent engineering problem.

SQLite research changed implementation: the initial Debian 3.46.1 library fell in the
upstream affected range for the WAL-reset race. Final builds compile checksum-verified
SQLite 3.54.0, including sanitizer instrumentation. Initial validation and measurements
were superseded by fresh tests and reports. No claim is made that our small test suite
would reproduce that upstream race.

Follow-up: Reuters' public site-search endpoint again returned HTTP 401. Bing and Yahoo
search requests returned HTTP 403. Their hostnames were added to the environment draft
without removing existing entries. No search snippet, article date or unverified corporate
claim was promoted to an established fact. Recheck after relevant environment updates.
