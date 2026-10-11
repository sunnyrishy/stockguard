'use strict';
// Educational browser model. The local site uses C++/SQLite instead of this model.
// State is session-only. This is not a durable inventory service.
window.createStockGuardSimulator = () => {
  const products = [{id:'milk',name:'Whole milk',category:'dairy',unit:'carton',status:'active'}];
  const inventory = [{product_id:'milk',location_id:'demo',quantity:100,version:1,updated_at:Math.floor(Date.now()/1000)}];
  const history=[], reviews=[], audit=[{id:1,kind:'initialize',product_id:'milk',location_id:'demo',previous_quantity:null,new_quantity:100,version:1,actor:'demo-seed'}];
  const movements=new Map();
  const fail = message => {throw new Error(message);};
  const integer=(v,min=0,max=1000000000)=>Number.isSafeInteger(v)&&v>=min&&v<=max;
  const state=(p,l)=>inventory.find(x=>x.product_id===p&&x.location_id===l);
  const active=p=>products.some(x=>x.id===p&&x.status==='active');
  const canonical=v=>JSON.stringify(v&&typeof v==='object'?(Array.isArray(v)?v.map(x=>JSON.parse(canonical(x))):Object.fromEntries(Object.keys(v).sort().map(k=>[k,JSON.parse(canonical(v[k]))]))):v);
  const change=(s,q,kind,id,actor)=>{audit.push({id:audit.length+1,kind,event_id:id,product_id:s.product_id,location_id:s.location_id,previous_quantity:s.quantity,new_quantity:q,version:s.version+1,actor});s.quantity=q;s.version++;s.updated_at=Math.floor(Date.now()/1000);};
  const page=(rows,r)=>rows.slice(r.offset||0,(r.offset||0)+(r.limit||1000));
  return r=>{
    const now=Math.floor(Date.now()/1000);
    switch(r.op){
    case 'products':return page(r.id?products.filter(x=>x.id===r.id):products,r);
    case 'inventory':return page(inventory,r);
    case 'audit':return page(audit,r);
    case 'history':return page(history.map(x=>x.result),r);
    case 'pending':return page(history.filter(x=>['REVIEW','DEFER'].includes(x.result.status)&&!reviews.some(y=>y.observation_id===x.o.id)).map(x=>({...x.result,product_id:x.o.product_id,location_id:x.o.location_id})),r);
    case 'reviews':return page(reviews.map(x=>x.result),r);
    case 'metrics':return {observations:history.length,pending:history.filter(x=>['REVIEW','DEFER'].includes(x.result.status)&&!reviews.some(y=>y.observation_id===x.o.id)).length,statuses:history.reduce((a,x)=>(a[x.result.status]=(a[x.result.status]||0)+1,a),{})};
    case 'product_put':{
      if(!r.id||!r.name||!r.unit||!['active','inactive'].includes(r.status))fail('Provide a valid product, unit and status.');
      const p=products.find(x=>x.id===r.id);
      if(p&&p.unit!==r.unit&&inventory.some(x=>x.product_id===r.id))fail('Units cannot change after inventory exists.');
      const value={id:r.id,name:r.name,category:r.category,unit:r.unit,status:r.status};if(p)Object.assign(p,value);else products.push(value);return value;
    }
    case 'initialize':{
      if(!active(r.product_id)||!r.location_id||!integer(r.quantity)||!r.actor)fail('Use an active product, location, nonnegative count and actor.');
      if(state(r.product_id,r.location_id))fail('Inventory already exists.');
      const s={product_id:r.product_id,location_id:r.location_id,quantity:r.quantity,version:1,updated_at:now};inventory.push(s);audit.push({id:audit.length+1,kind:'initialize',product_id:s.product_id,location_id:s.location_id,previous_quantity:null,new_quantity:s.quantity,version:1,actor:r.actor});return s;
    }
    case 'movement':{
      const saved=movements.get(r.id);if(saved){if(saved.payload!==canonical(r))fail('Movement ID conflict.');return {...saved.result,replayed:true};}
      const s=state(r.product_id,r.location_id);if(!s||!active(r.product_id)||!r.id||!r.actor||!integer(r.delta,-1000000000)||r.expected_version!==s.version||!integer(s.quantity+r.delta))fail('Check product, count and current version.');
      change(s,s.quantity+r.delta,'movement',r.id,r.actor);const result={quantity:s.quantity,version:s.version,replayed:false};movements.set(r.id,{payload:canonical(r),result});return result;
    }
    case 'ingest':{
      const o=r.observation;
      if(!o||!o.id||!o.scan_id||!o.product_id||!o.location_id||!integer(o.quantity)||!Number.isFinite(o.confidence)||o.confidence<0||o.confidence>1||typeof o.complete!=='boolean'||!integer(o.expected_version,1,1000000000000)||!integer(o.observed_at,0,253402300799))fail('Invalid observation fields.');
      const saved=history.find(x=>x.o.id===o.id);if(saved){if(canonical(saved.o)!==canonical(o))fail('Observation ID conflict.');return {...saved.result,replayed:true};}
      const s=state(o.product_id,o.location_id);let status='ACCEPT',reason='consistent_count';
      if(history.some(x=>x.o.scan_id===o.scan_id&&x.o.product_id===o.product_id&&x.o.location_id===o.location_id)){status='REJECT';reason='duplicate_scan';}
      else if(!active(o.product_id)){status='REJECT';reason='invalid_product';}
      else if(!s){status='DEFER';reason='missing_inventory';}
      else if(o.observed_at>now+5){status='REJECT';reason='future_observation';}
      else if(now-o.observed_at>300||o.observed_at<s.updated_at){status='REJECT';reason='stale_observation';}
      else if(o.expected_version!==s.version){status='DEFER';reason='version_conflict';}
      else if(!o.complete){status='DEFER';reason='incomplete_coverage';}
      else if(o.confidence<.95){status='REVIEW';reason='low_confidence';}
      else if(Math.abs(o.quantity-s.quantity)>Math.max(2,.05*s.quantity)){status='REVIEW';reason='large_discrepancy';}
      const result={id:o.id,status,reason,replayed:false,evidence:{expected_quantity:s?.quantity??null,observed_quantity:o.quantity,confidence:o.confidence,complete:o.complete,expected_version:o.expected_version}};
      history.push({o:structuredClone(o),result});if(status==='ACCEPT')change(s,o.quantity,'observation',o.id,'demo-scanner');return result;
    }
    case 'review':{
      if(!r.actor||!['correct','confirm','reject'].includes(r.action))fail('Choose a review action and actor.');
      const saved=reviews.find(x=>x.observation_id===r.observation_id);if(saved){if(saved.payload!==canonical(r))fail('Review conflict.');return {...saved.result,replayed:true};}
      const entry=history.find(x=>x.o.id===r.observation_id);if(!entry||!['REVIEW','DEFER'].includes(entry.result.status))fail('Choose a pending observation.');
      const s=state(entry.o.product_id,entry.o.location_id);
      if(r.action!=='reject'){
        if(!s||!active(entry.o.product_id)||r.expected_version!==s.version)fail('Use an active product and current inventory version.');
        if(!integer(r.verified_at,0,253402300799)||r.verified_at<s.updated_at||r.verified_at>now+5||now-r.verified_at>300)fail('Perform a fresh complete count.');
        const q=r.action==='correct'?r.quantity:entry.o.quantity;if(!integer(q))fail('Provide a nonnegative integer count.');change(s,q,'review',r.observation_id,r.actor);
      }
      const result={observation_id:r.observation_id,action:r.action,actor:r.actor,quantity:s?.quantity??null,version:s?.version??null,replayed:false};reviews.push({observation_id:r.observation_id,payload:canonical(r),result});return result;
    }
    default:fail('This operation is not available in the educational browser demo.');
    }
  };
};
