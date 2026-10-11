# Third-party dependencies
Dependencies are downloaded into ignored build directories, not vendored into source.
Retain their license notices when redistributing binaries or source bundles.

| Dependency | Pin | License | Purpose |
| --- | --- | --- | --- |
| nlohmann JSON | 3.11.3, commit 9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03 | MIT | Command parsing and serialization |
| GoogleTest | 1.16.0, commit 6910c9d9165801d8827d628cb72eb7ea9dd538c5 | BSD-3-Clause | Tests only |
| SQLite | 3.54.0, official amalgamation SHA256 pinned | Public domain | Durable state |
| CMake | Cloud tools 3.31.6 | BSD-3-Clause | Build tooling |
| Ninja | Cloud tools 1.11.1.4 | Apache-2.0 | Build tooling |
| clang-format | Cloud tools 19.1.7 | Apache-2.0 with LLVM exceptions | Formatting |
| clang-tidy | Cloud tools 19.1.0.1 | Apache-2.0 with LLVM exceptions | Static analysis |

Archive SHA256 values are checked for CMake dependencies and the official SQLite archive.
TLS verification remains enabled. Default builds compile the same pinned SQLite source on Linux and WSL. An explicit system
SQLite override requires version 3.51.3 or newer, including the WAL-reset fix.
