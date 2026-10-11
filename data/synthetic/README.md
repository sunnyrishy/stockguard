# Synthetic data
`sample.jsonl` contains ten labeled independent snapshots from the Python generator.
Generate more with `python3 scripts/generate-dataset.py --count 10000 --seed 2026`.
Do not replay these as sequential production observations: each snapshot assumes a fresh
ledger at quantity 100, version 1 and time 1000. The C++ benchmark uses std::mt19937 with
seed 2026 and the same scenario definitions; its sequence differs from Python's PRNG.
The benchmark reports its actual decisions and ground-truth counts. Neither dataset is
representative evidence about a real store or image recognition model.
