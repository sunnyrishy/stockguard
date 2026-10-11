'use strict';
let ledger = [], lastScan;
const byId = id => document.getElementById(id);
const now = () => Math.floor(Date.now() / 1000);
const unique = () => crypto.randomUUID();
const simulated = window.createStockGuardSimulator();
const backendMode = fetch('./health').then(async response => {
  if (!response.ok) return false;
  const data = await response.json();
  return data.ok === true && data.application === 'StockGuard';
}).catch(() => false).then(live => {
  byId('connection').textContent = live ? 'LIVE ENGINE / C++ + SQLite / persistent local state' : 'BROWSER SIMULATION / educational policy model / session-only state';
  return live;
});
async function api(request) {
  if (!await backendMode) return structuredClone(simulated(request));
  const response = await fetch('./api', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(request)});
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
  if (id === 'pending') {const cell=document.createElement('th');cell.textContent='Action';head.append(cell);}
  element.append(head);
  rows.forEach(row => {const tr = document.createElement('tr'); keys.forEach(key => {const cell = document.createElement('td'); cell.textContent = typeof row[key] === 'object' ? JSON.stringify(row[key]) : String(row[key]); tr.append(cell);}); if (id === 'pending') {const cell=document.createElement('td');const button=document.createElement('button');button.textContent='Review';button.onclick=()=>{const form=byId('review');const current=ledger.find(x=>x.product_id===row.product&&x.location_id===row.location);form.elements.observation_id.value=row.id;form.elements.expected_version.value=current?.version??'';form.elements.quantity.value=current?.quantity??0;form.elements.action.value='correct';updateReviewFields();form.scrollIntoView({block:'center'});};cell.append(button);tr.append(cell);}element.append(tr);});
  host.append(element);
}
async function refresh() {
  const [inventory, pending, audit, products, metrics] = await Promise.all(['inventory', 'pending', 'audit', 'products', 'metrics'].map(op => api({op})));
  ledger = inventory;
  const main = inventory.find(x => x.product_id === 'milk' && x.location_id === 'demo');
  for (const [id, value] of [['trusted-count',main?.quantity ?? '?'],['trusted-version',main?.version ?? '?'],['pending-count',pending.length]]) {
    const el = byId(id); if (el.textContent !== String(value)) {el.textContent = value;el.classList.remove('bump');void el.offsetWidth;el.classList.add('bump');}
  }
  byId('shelf-count').textContent = `${main?.quantity ?? 0} units`;
  if (pending.length && !pending.some(x=>x.id===byId('review').elements.observation_id.value)) {byId('review').elements.observation_id.value = pending[0].id;byId('review').elements.expected_version.value = main?.version ?? '';}
  table('inventory', inventory); table('pending', pending.map(d => ({id: d.id, product: d.product_id, location: d.location_id, status: d.status, reason: d.reason})));
  table('audit', audit); table('products', products);
  if (!pending.length) byId('review').elements.observation_id.value='';
  byId('metrics').textContent = JSON.stringify(metrics, null, 2);
}
async function run(action) {
  if (document.body.classList.contains('busy')) return;
  document.body.classList.add('busy');
  document.querySelectorAll('button:not(#motion-toggle)').forEach(b => b.disabled = true);
  try {byId('message').textContent = ''; await action(); await refresh();}
  catch (error) {byId('message').textContent = error.message;}
  finally {document.body.classList.remove('busy', 'processing'); document.querySelectorAll('button:not(#motion-toggle)').forEach(b => b.disabled = false);}
}
const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');
let paused = reducedMotion.matches;
function motionLabel() {document.documentElement.classList.toggle('motion-paused', paused);byId('motion-toggle').textContent = paused ? 'Resume motion' : 'Pause motion';byId('motion-toggle').setAttribute('aria-pressed', String(paused));}
byId('motion-toggle').onclick = () => {paused = !paused;motionLabel();};
reducedMotion.addEventListener('change', event => {paused=event.matches;motionLabel();});
motionLabel();
async function animateScan() {
  document.body.classList.add('processing');
  byId('outcome-badge').textContent = 'CHECKING EVIDENCE';
  for (let step=0;step<4;step++) {
    document.querySelectorAll('.station').forEach((el,i)=>el.classList.toggle('active',i<=step));
    byId('scan-token').style.left = `${6 + step*25}%`;
    if (!paused && !reducedMotion.matches) await new Promise(resolve=>setTimeout(resolve,300));
  }
}
function explain(result) {
  const status=result.status || (result.action === 'reject' ? 'REJECT' : 'ACCEPT');
  byId('outcome-badge').textContent=result.replayed ? 'SAFE REPLAY' : status;
  byId('outcome-badge').dataset.status=status;
  const descriptions={ACCEPT:['Count accepted. Ledger protected.','The count passed the rules. Its update and audit committed together.'],REVIEW:['Pause the update. Ask a human.','Confidence or discrepancy needs a second look. Trusted inventory stays unchanged.'],DEFER:['Hold the scan. Keep the ledger.','Complete coverage and current inventory are needed before this count can update stock.'],REJECT:['Stop the scan. Preserve the count.','This evidence cannot update inventory. Its reason stays visible.']};
  const copy=result.replayed ? ['Same scan. Same result.','The saved decision returned without another inventory update or audit.'] : result.action ? [result.action==='reject'?'Evidence closed. Inventory unchanged.':'Review complete. Count updated.',result.action==='reject'?'The item leaves the queue without changing trusted stock.':'A fresh verified count and its audit were saved together.'] : descriptions[status];
  byId('result-title').textContent=copy[0];byId('result-description').textContent=copy[1];
}
async function ingest(observation) {
  lastScan = observation;
  await animateScan();
  const result=await api({op:'ingest',observation});
  byId('decision').textContent=JSON.stringify(result,null,2);explain(result);
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
      const result = await api(data);byId('decision').textContent = JSON.stringify(result, null, 2);
      if (id === 'review') explain(result);
    }
  });};
}
run(refresh);
fetch('./benchmarks').then(r => r.json()).then(data => {delete data.quality_examples; byId('benchmark').textContent = JSON.stringify(data, null, 2);}).catch(() => {byId('benchmark').textContent = 'No report available.';});
