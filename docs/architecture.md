# Architecture
The C++ library owns policy evaluation and persistence. A JSON-lines executable provides
one integration boundary. The standard-library Python web demo invokes that executable
with a bounded input, timeout and fixed argument list. It contains no reconciliation logic.
Each process uses its own SQLite connection. SQLite serializes writers across processes.
There is no persistent worker pool: SQLite has one writer, and the bounded local HTTP
server uses a fixed concurrency limit. Batch ingestion is synchronous with per-item commits.

```mermaid
flowchart LR
    Browser --> Demo[Loopback Python demo]
    Demo --> CLI[JSON-lines C++ executable]
    CLI --> Service[Application service]
    Service --> Engine[Pure policy evaluation]
    Service --> DB[(SQLite WAL)]
    Tests --> Engine
    Tests --> Service
```

```mermaid
sequenceDiagram
    participant C as Caller
    participant S as Application service
    participant D as SQLite
    participant E as Policy
    C->>S: Observation and expected version
    S->>D: BEGIN IMMEDIATE
    S->>D: Lookup persisted ID and scan key
    S->>D: Read catalog and inventory with movements applied
    S->>E: Evaluate immutable evidence
    E-->>S: Decision and reason
    S->>D: Store observation and decision
    opt Accepted
        S->>D: Compare version, update inventory, insert audit
    end
    S->>D: COMMIT
    S-->>C: Durable decision
```

An event is received, validated, evaluated and committed as one transaction. REVIEW and
DEFER preserve trusted state. A human verification is a later transaction with a captured
version check. Query requests run in a consistent transaction too. RAII closes statements,
connections and unfinished transactions. Prepared statements bind all caller values.
