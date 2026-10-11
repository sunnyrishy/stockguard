# Command example

This runnable example uses a new database and the actual current time:

```sh
mkdir -p .runtime
python3 - <<'PY' | build/stockguard --db .runtime/example.db
import json, time
requests = [
    dict(op='product_put', id='milk', name='Whole milk', category='dairy',
         unit='carton', status='active'),
    dict(op='initialize', product_id='milk', location_id='store', quantity=100, actor='seed'),
    dict(op='ingest', observation=dict(id='scan-1', scan_id='batch-1', product_id='milk',
         location_id='store', quantity=101, confidence=0.99, complete=True,
         expected_version=1, observed_at=int(time.time()), source='synthetic')),
    dict(op='inventory'),
    dict(op='audit'),
]
for request in requests:
    print(json.dumps(request))
PY
```

Expect ACCEPT, quantity 101, version 2 and two audit rows. Initialization refuses to overwrite
an existing record, so use a fresh database path to repeat this example. Replaying the same
observation returns the original decision without an extra inventory change. Full request
schemas, pagination, errors and partial batch semantics are in [API documentation](api.md).
A custom policy can be supplied with `--policy config/policy.json`.

