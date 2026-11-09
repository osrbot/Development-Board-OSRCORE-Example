import test from 'node:test';
import assert from 'node:assert/strict';
import {createParameterChannel,sameParameters} from '../.vitepress/theme/runtime-parameters.mjs';
const tick=()=>new Promise(r=>setTimeout(r,0));
function fixture(timeoutMs=100){let sent;const channel=createParameterChannel(line=>{sent=JSON.parse(line.slice('osrcore params '.length));},{timeoutMs});return {channel,get sent(){return sent;},reply(extra={}){channel.receive('OSRCORE_PARAMS '+JSON.stringify({id:sent.id,project:sent.lesson,ok:true,values:sent.values,saved:sent.save,...extra}));}};}
test('parameter application waits for a matching device acknowledgement',async()=>{
 const f=fixture();let complete=false;const task=f.channel.request(1,{interval:100,brightness:80}).then(r=>{complete=true;return r;});await tick();
 assert.equal(complete,false);assert.equal(f.sent.lesson,'01_blink_led');
 f.reply({id:'stale'});await tick();assert.equal(complete,false);
 f.reply();assert.equal((await task).values.brightness,80);
});
test('wrong lesson, rejected values and mismatched echo cannot report success',async()=>{
 for(const reply of [{project:'02_buzzer'},{ok:false,error:'parameter_out_of_range'},{values:{interval:500,brightness:32}},{values:{interval:100,brightness:80,extra:2}}]){
  const f=fixture();const task=f.channel.request(1,{interval:100,brightness:80});const rejected=assert.rejects(task);await tick();f.reply(reply);await rejected;
 }
});
test('save needs persistence acknowledgement, not just accepted RAM values',async()=>{
 const f=fixture();const task=f.channel.request(7,{kp:3},true);const rejected=assert.rejects(task,/response_mismatch/);await tick();f.reply({saved:false});await rejected;
 const next=f.channel.request(7,{kp:3},true);await tick();f.reply();assert.equal((await next).saved,true);
});
test('timeouts and disconnections cancel pending requests; late acknowledgements are ignored',async()=>{
 const f=fixture(20);await assert.rejects(f.channel.request(1,{interval:100,brightness:80}),/response_timeout/);f.reply();
 const task=f.channel.request(1,{interval:200,brightness:70});const rejected=assert.rejects(task,/device_disconnected/);await tick();f.channel.cancel();await rejected;
});
test('simulation inputs and invalid values never reach USB',async()=>{
 let writes=0;const channel=createParameterChannel(()=>writes++);
 await assert.rejects(channel.request(5,{angle:20}));
 await assert.rejects(channel.request(3,{pulse:2500}));
 await assert.rejects(channel.request(1,{interval:101,brightness:32}));
 assert.equal(writes,0);
 assert.equal(sameParameters({brightness:80},{brightness:32}),false);
 assert.equal(sameParameters({alpha:.95},{alpha:.9500000001}),true);
});

test('firmware and browser expose the same parameter ranges and steps',async()=>{
 const {readFile}=await import('node:fs/promises');
 const {firmwareParameters,exampleDirs}=await import('../.vitepress/theme/lesson-contract.mjs');
 for(const [lesson,definitions] of Object.entries(firmwareParameters)){
  const source=await readFile(new URL('../../'+exampleDirs[lesson]+'/main/main.c',import.meta.url),'utf8');
  for(const [key,definition] of Object.entries(definitions)){
   const match=source.match(new RegExp('\\{\\s*"'+key+'"\\s*,\\s*([\\d.]+)\\s*,\\s*([\\d.]+)\\s*,\\s*([\\d.]+)\\s*,\\s*OSR_'+definition.macro+'\\s*\\}'));
   assert.ok(match,`${lesson} ${key} is registered with the expected default`);
   assert.deepEqual(match.slice(1).map(Number),[definition.min,definition.max,definition.step]);
   assert.ok(source.includes(`osrcore_param_get("${key}"`)||source.includes(`?"${key}"`)||source.includes(`:"${key}"`),`${key} is read at runtime`);
  }
 }
});
