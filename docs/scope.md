# Release scope
A modular C++20 monolith with SQLite, a JSON command interface and a local browser demo.
Counts are nonnegative integer units for one product at one location, never shelf fragments.
Every scan supplies the inventory version on which it was based and a coverage flag.
Movements are trusted signed unit changes, applied separately with their own event IDs.
Current inventory already includes recorded movements. Older observations are rejected;
we do not reconstruct historical inventory or infer unrecorded sales.

The initial release supports catalog management, inventory initialization, movement recording,
observation ingestion, partial batches, review, audit queries and metrics. Reviewers must
supply the captured version and a current verification timestamp. Unknown inventory is deferred.
No image model, automatic product substitution inference, distributed service, production
identity provider, external store integration or physical inventory accuracy guarantee.
Confidence is an uncalibrated model signal, not a probability of correctness.
