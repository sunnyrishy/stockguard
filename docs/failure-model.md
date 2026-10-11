# Failure model
| Failure | Behavior |
| --- | --- |
| Exception after observation insertion | RAII rolls back observation and decision |
| Exception after inventory update | Inventory, event and audit all roll back |
| Exception before commit | All writes roll back |
| Death in an uncommitted transaction | SQLite rolls back on reconnect |
| Lost response after commit | Replay returns the saved original decision |
| Concurrent duplicate | Unique ID plus serialized transaction prevents a second update |
| Conflicting payload | Error, original data preserved |
| Review version conflict | Error, pending event and inventory unchanged |
| Disk full or busy timeout | Database error, caller retries the same event ID |
| Corrupt database | Error, no automatic destructive repair |

WAL and synchronous=FULL support SQLite durability within operating-system and storage
hardware guarantees. This is not a replicated database or a backup strategy. SIGKILL tests
exercise process death, not physical power loss. Migrations use user_version under a
transaction and refuse newer schemas. Test-only injectable checkpoints are inaccessible
from the command interface.
