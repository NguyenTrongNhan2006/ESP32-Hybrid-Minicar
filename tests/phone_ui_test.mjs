import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const html = fs.readFileSync(new URL('../firmware/esp-idf/main/web.html', import.meta.url), 'utf8');
const js = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const elements = new Map(), timers = [], listeners = new Map(), requests = [];
let server = {state:'STOPPED',owner:0,epoch:0,battery_mv:12600,last_stop:'BOOT',message:'Ready'};
let sequence = 0;
function element(id) {
  if (!elements.has(id)) elements.set(id,{value:id==='steer'?'90':'0',textContent:'',disabled:true,
    classList:{add(){},remove(){}},setPointerCapture(){},handlers:{},
    addEventListener(name,callback){this.handlers[name]=callback;}});
  return elements.get(id);
}
const context = vm.createContext({
  Uint32Array,Map,Promise,Error,Date,AbortController,console,
  crypto:{getRandomValues(array){array[0]=12345;return array;}},
  document:{hidden:false,getElementById:element,addEventListener(name,callback){listeners.set(name,callback);}},
  window:{addEventListener(name,callback){listeners.set(name,callback);}},
  setTimeout,clearTimeout,setInterval(callback,ms){timers.push({callback,ms});return timers.length;},
  fetch:async (url,options={})=>{
    if(url==='/api/status') return {ok:true,json:async()=>({...server})};
    requests.push({body:options.body,headers:options.headers});
    const current = Number(options.headers['X-Control-Seq']);
    assert(current>sequence);sequence=current;
    if(options.body==='ARM'){server.state='ARMED';server.owner=12345;}
    if(options.body==='RUN')server.state='RUNNING';
    if(options.body==='STOP'){server.state='STOPPED';server.owner=0;server.epoch++;}
    return {ok:true};
  }
});
vm.runInContext(js,context);
const flush=()=>new Promise(resolve=>setImmediate(resolve));
const timer=ms=>timers.find(x=>x.ms===ms).callback;
await flush(); assert.equal(element('state').textContent,'STOPPED');
await timer(150)();await timer(100)();await flush();
assert.equal(requests.length,0,'Status polling must not feed heartbeat');
element('arm').onclick();await flush();await timer(150)();
assert.equal(server.state,'ARMED');assert.equal(element('hold').disabled,false);
element('hold').onpointerdown({preventDefault(){},pointerId:1});await flush();await timer(150)();
assert.equal(server.state,'RUNNING');assert.equal(element('gas').disabled,false);
timer(100)();await flush();assert.equal(requests.at(-1).body,'PING');
element('gas').value='30';element('gas').oninput();timer(80)();await flush();
assert.equal(requests.at(-1).body,'GAS 30');
element('hold').handlers.pointerup();await flush();assert.equal(requests.at(-1).body,'STOP');
const count=requests.length;timer(100)();await flush();assert.equal(requests.length,count);
await timer(150)();element('arm').onclick();await flush();await timer(150)();
element('hold').onpointerdown({preventDefault(){},pointerId:2});await flush();await timer(150)();
context.document.hidden=true;listeners.get('visibilitychange')();await flush();
assert.equal(requests.at(-1).body,'STOP');assert.equal(element('gas').disabled,true);
context.document.hidden=false;await timer(150)();
// Queued motion is cancelled if STOP happens before its promise is dispatched.
vm.runInContext('held=true;send("ESC 100");stop();',context);await flush();
assert.equal(requests.at(-1).body,'STOP');assert(!requests.some(x=>x.body==='ESC 100'));
assert(requests.every(x=>x.headers['X-Control-Session']==='12345' && x.headers['X-Control-Epoch']!==undefined));
console.log('PASS phone UI: status-only polling, ARM/hold RUN, heartbeat, sliders, release/background STOP, stale UI queue and frame headers');
