'use strict';
let ledger = [], lastScan;
const byId = id => document.getElementById(id);
const now = () => Math.floor(Date.now() / 1000);
const unique = () => crypto.randomUUID();
async function api(request) {
  const response = await fetch('/api', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(request)});
  const data = await response.json();
  if (!data.ok) throw new Error(`${data.error}: ${data.message || ''}`);
  return data.result;
}
function table(id, rows) {
  const host = byId(id); host.replaceChildren();
  if (!rows.length) {host.textContent = 'No records.'; return;}
  const element = document.createElement('table');
  const keys = Object.keys(rows[0]);
  const head = document.createElement('tr');
  keys.forEach(key => {const cell = document.createElement('th'); cell.textContent = key; head.append(cell);});
  element.append(head);
  rows.forEach(row => {const tr = document.createElement('tr'); keys.forEach(key => {const cell = document.createElement('td'); cell.textContent = typeof row[key] === 'object' ? JSON.stringify(row[key]) : String(row[key]); tr.append(cell);}); element.append(tr);});
  host.append(element);
}
async function refresh() {
  const [inventory, pending, audit, products, metrics] = await Promise.all(['inventory', 'pending', 'audit', 'products', 'metrics'].map(op => api({op})));
  ledger = inventory;
  table('inventory', inventory); table('pending', pending.map(d => ({id: d.id, product: d.product_id, location: d.location_id, status: d.status, reason: d.reason})));
  table('audit', audit); table('products', products);
  byId('metrics').textContent = JSON.stringify(metrics, null, 2);
}
async function run(action) {
  try {byId('message').textContent = ''; await action(); await refresh();}
  catch (error) {byId('message').textContent = error.message;}
}
async function ingest(observation) {
  lastScan = observation;
  byId('decision').textContent = JSON.stringify(await api({op: 'ingest', observation}), null, 2);
}
byId('refresh').onclick = () => run(refresh);
document.querySelectorAll('[data-scenario]').forEach(button => {button.onclick = () => run(async () => {
  await refresh(); const state = ledger.find(row => row.product_id === 'milk' && row.location_id === 'demo');
  if (!state) throw new Error('Initialize milk at demo first.');
  const id = unique();
  const o = {id, scan_id: `scan-${id}`, product_id: 'milk', location_id: 'demo', quantity: state.quantity,
    confidence: 0.99, complete: true, expected_version: state.version, observed_at: now(), source: 'demo-simulator'};
  switch (button.dataset.scenario) {
  case 'low': o.confidence = 0.5; break;
  case 'large': o.quantity += Math.max(50, Math.ceil(state.quantity * 0.2)); break;
  case 'partial': o.complete = false; break;
  case 'stale': o.observed_at -= 600; break;
  case 'missing': o.product_id = 'unknown-product'; break;
  case 'version': o.expected_version += 1; break;
  }
  await ingest(o);
});});
byId('replay').onclick = () => run(async () => {if (!lastScan) throw new Error('Submit a scan first.'); await ingest(lastScan);});
function updateReviewFields() {
  const form = byId('review');
  const modifiesInventory = form.elements.action.value !== 'reject';
  form.elements.expected_version.disabled = !modifiesInventory;
  form.elements.expected_version.required = modifiesInventory;
  form.elements.quantity.disabled = form.elements.action.value !== 'correct';
  form.elements.quantity.required = form.elements.action.value === 'correct';
}
byId('review').elements.action.onchange = updateReviewFields;
updateReviewFields();
for (const id of ['scan', 'review', 'product', 'initialize', 'movement']) {
  byId(id).onsubmit = event => {event.preventDefault(); run(async () => {
    const data = Object.fromEntries(new FormData(event.target));
    for (const field of ['quantity', 'confidence', 'expected_version', 'delta']) if (field in data) data[field] = Number(data[field]);
    if (id === 'scan') {
      data.complete = data.complete === 'true'; data.id = unique(); data.scan_id = `scan-${data.id}`;
      data.observed_at = now(); data.source = 'demo-manual'; await ingest(data);
    } else {
      data.op = id === 'product' ? 'product_put' : id;
      if (id === 'review' && data.action !== 'reject') data.verified_at = now();
      if (id === 'initialize' || id === 'movement') data.actor = 'demo-operator';
      if (id === 'movement') data.id = unique();
      byId('decision').textContent = JSON.stringify(await api(data), null, 2);
    }
  });};
}
run(refresh);
fetch('/benchmarks').then(r => r.json()).then(data => {delete data.quality_examples; byId('benchmark').textContent = JSON.stringify(data, null, 2);}).catch(() => {byId('benchmark').textContent = 'No report available.';});
