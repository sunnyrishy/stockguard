const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
function setup() {
  const context={window:{},structuredClone,Date};
  vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../dashboard/simulator.js'),'utf8'),context);
  return context.window.createStockGuardSimulator();
}
const observation = (changes={}) => ({id:'one',scan_id:'scan-one',product_id:'milk',location_id:'demo',quantity:101,confidence:.99,complete:true,expected_version:1,observed_at:Math.floor(Date.now()/1000),...changes});
test('accept and replay update the ledger and audit exactly once',()=>{
  const api=setup(),o=observation();assert.equal(api({op:'ingest',observation:o}).status,'ACCEPT');
  assert.equal(api({op:'ingest',observation:o}).replayed,true);
  assert.equal(api({op:'inventory'})[0].version,2);assert.equal(api({op:'audit'}).length,2);
  assert.throws(()=>api({op:'ingest',observation:{...o,quantity:102}}),/conflict/);
  assert.equal(api({op:'audit'}).length,2);
});
test('review, defer and reject leave trusted stock unchanged',()=>{
  for(const changes of [{confidence:.94},{quantity:106},{complete:false},{expected_version:2},{observed_at:Math.floor(Date.now()/1000)-400},{product_id:'unknown'}]){
    const api=setup();assert.notEqual(api({op:'ingest',observation:observation(changes)}).status,'ACCEPT');
    assert.equal(api({op:'inventory'})[0].quantity,100);assert.equal(api({op:'inventory'})[0].version,1);assert.equal(api({op:'audit'}).length,1);
  }
});
test('movement protects its version and rejects duplicate payload conflicts',()=>{
  const api=setup(),r={op:'movement',id:'move',product_id:'milk',location_id:'demo',delta:-1,expected_version:1,actor:'tester'};
  api(r);assert.equal(api(r).replayed,true);
  const o=observation({observed_at:Math.floor(Date.now()/1000)});assert.equal(api({op:'ingest',observation:o}).status,'DEFER');
  assert.equal(api({op:'inventory'})[0].quantity,99);assert.throws(()=>api({...r,delta:1}),/conflict/);
});
test('missing-inventory evidence can close, but correction cannot bypass prerequisites',()=>{
  const api=setup();api({op:'ingest',observation:observation({location_id:'missing'})});
  const review={op:'review',observation_id:'one',actor:'tester',action:'correct',quantity:1,expected_version:1,verified_at:Math.floor(Date.now()/1000)};
  assert.throws(()=>api(review));const reject={op:'review',observation_id:'one',actor:'tester',action:'reject'};
  assert.equal(api(reject).quantity,null);assert.equal(api(reject).replayed,true);
  assert.equal(api({op:'pending'}).length,0);assert.equal(api({op:'audit'}).length,1);
});
