# Portfolio materials

The project is on [GitHub](https://github.com/sunnyrishy/stockguard).
The LinkedIn text below is a draft. Posting it or changing repository visibility needs separate approval.

## LinkedIn draft

I built StockGuard, a C++20 project that checks scanner counts before updating inventory.

It flags uncertain counts for human review, prevents older scans from overwriting newer
stock movements, and saves inventory changes with their audit records in SQLite.
Repeating the same request returns the saved result.

The project has 33 C++ tests covering decision rules, concurrent requests, rollback and
restart recovery. I also tested the browser demo and ran sanitizer checks.

One lesson stood out: a consistent database does not guarantee a correct physical count.
On 10,000 synthetic snapshots, the policy flagged 4,031 incorrect counts but still accepted
1,039. The repository includes those results and their limitations.

Code, diagrams and demo: https://github.com/sunnyrishy/stockguard

Independent project using synthetic data, with no real-store deployment.

## Resume bullets

- Built a C++20 inventory reconciliation engine with SQLite transactions, replay protection
  and version-checked human corrections.
- Verified rollback, restart recovery and concurrent requests with 33 C++ tests; ran
  ASan/UBSan and scoped TSan with a documented SQLite suppression.
- Compared four policies on 10,000 labeled synthetic snapshots; flagged 4,031 of 5,070
  incorrect counts and reported all 1,039 missed errors.

## 30-second introduction

StockGuard checks scanner counts before they change inventory. It accepts counts that pass
its rules and keeps uncertain ones for human review. SQLite saves each accepted change and
its audit record together. Version checks protect newer stock movements, and duplicate
requests replay safely. It is a tested local demo using synthetic data.

## Interview talking points

| Question | Answer |
| --- | --- |
| Why check more than confidence? | Confidence alone does not prove the count is fresh, complete or correct. |
| What if a response is lost? | Retry the same ID and payload to retrieve the saved result. |
| How do changes stay linked to audits? | Both commit in one transaction or roll back together. |
| Can SQLite run concurrent writers? | It allows one writer at a time; requests use a bounded wait. |
| What does ACCEPT prove? | The evidence passed the policy, not that the physical count is true. |
| Can a pending item be rejected without inventory? | Yes. Rejection closes evidence without changing inventory. |

For technical detail, start with [architecture](architecture.md),
[decision rules](reconciliation-design.md), [concurrency](concurrency-design.md),
and the [sanitizer note](decisions/002-sqlite-sanitizers.md).

## GitHub profile summary

C++20 inventory reconciliation with SQLite, replay protection, human review, 33 tests,
a browser demo and documented synthetic benchmarks.
