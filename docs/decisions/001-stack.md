# ADR 001: C++ library, SQLite and a command boundary
Accepted. C++20 keeps policy independently testable. SQLite supplies atomic persistence and
cross-process writer coordination without operational services. JSON lines makes a simple
bounded subprocess boundary portable to WSL2. The local dashboard uses Python's standard
library, avoiding a second package ecosystem and a binding ABI. Costs include process
startup and serialization overhead, and no persistent HTTP backend connection pool.
GoogleTest is the test framework. nlohmann JSON supplies parsing. Both are pinned by commit.
A custom benchmark harness records end-to-end latency and labeled synthetic comparisons.
