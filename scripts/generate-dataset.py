#!/usr/bin/env python3
"""Generate labeled independent snapshots, not a sequential ledger replay."""
import argparse
import json
import random

parser = argparse.ArgumentParser()
parser.add_argument('--count', type=int, default=1000)
parser.add_argument('--seed', type=int, default=2026)
args = parser.parse_args()
if not 1 <= args.count <= 1000000:
    parser.error('count must be 1 to 1000000')
rng = random.Random(args.seed)
for i in range(args.count):
    scenario = rng.randrange(10)
    o = dict(id=f'o{i}', scan_id=f's{i}', product_id='milk', location_id='store', quantity=100,
             confidence=0.99, observed_at=1000, expected_version=1, complete=True, source='synthetic')
    if scenario == 0:
        o['quantity'] = 160
    elif scenario == 1:
        o.update(quantity=0, confidence=0.5)
    elif scenario == 2:
        o.update(quantity=80, complete=False)
    elif scenario == 3:
        o.update(quantity=90, observed_at=600)
    elif scenario == 4:
        o['quantity'] = 102
    elif scenario == 5:
        o['confidence'] = 0.6
    print(json.dumps(dict(observation=o, ground_truth=100, incorrect=scenario < 5,
                          scenario=scenario, seed=args.seed), sort_keys=True))
