import test from 'node:test';
import assert from 'node:assert/strict';
import http from 'node:http';
import {initialValues} from '../docs/.vitepress/theme/lab-simulation.mjs';
import {handle,privateAddress,validateDevice} from './lab-server.mjs';
import {validateFirmwareRequest,configHeader,firmwareValues,exampleDirs} from '../docs/.vitepress/theme/lesson-contract.mjs';
test('every lesson has a validated firmware contract',()=>{
 for(let n=1;n<exampleDirs.length;n++){const r=validateFirmwareRequest(n);assert.equal(r.example,exampleDirs[n]);assert.deepEqual(validateFirmwareRequest(n,firmwareValues(n,initialValues(n))).parameters,r.parameters);assert.match(configHeader(n,r.parameters),/^#pragma once/);}
 assert.deepEqual(firmwareValues(1,{interval:500,brightness:32,fakeSensor:20}),{interval:500,brightness:32});
 for(const values of [{interval:null},{interval:101},{interval:NaN},{interval:Infinity},{interval:'500'},{interval:99},{interval:2001},{brightness:256},{evil:'; exec'}])assert.throws(()=>validateFirmwareRequest(1,values));
 for(const n of [0,14,-1,1.2,'1'])assert.throws(()=>validateFirmwareRequest(n));
 assert.throws(()=>validateFirmwareRequest(4,{channel:1500}));
});
test('device proxy accepts only literal private IPv4 and pairing tokens',()=>{
 for(const host of ['10.1.2.3','172.16.0.2','172.31.255.2','192.168.99.1'])assert.equal(privateAddress(host),true);
 for(const host of ['127.0.0.1','localhost','8.8.8.8','169.254.169.254','192.168.999.1','172.32.0.1','192.168.1.2/path','192.168.1.2:80','192.168.1.2@evil.test'])assert.equal(privateAddress(host),false);
 assert.throws(()=>validateDevice({address:'192.168.1.2',token:'invalid'}));
 assert.equal(validateDevice({address:'192.168.1.2',token:'a'.repeat(32)}).address,'192.168.1.2');
});
test('local API rejects cross-origin mutations and invalid builds before compilation',async()=>{
 const server=http.createServer(handle);await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const base='http://127.0.0.1:'+server.address().port;
 try{
  assert.equal((await fetch(base+'/api/lab/health')).status,200);
  assert.equal((await fetch(base+'/api/lab/builds',{method:'POST',body:'{}'})).status,403);
  assert.equal((await fetch(base+'/api/lab/builds',{method:'POST',headers:{'X-OSRCORE-Lab':'1',Origin:'https://external.test'},body:'{}'})).status,403);
  const r=await fetch(base+'/api/lab/builds',{method:'POST',headers:{'X-OSRCORE-Lab':'1'},body:JSON.stringify({lesson:1,parameters:{interval:'malformed'}})});
  assert.equal(r.status,400);assert.match((await r.json()).error,/interval/);
  assert.equal((await fetch(base+'/api/lab/missing')).status,404);
  assert.equal((await fetch(base+'/missing-page')).status,404);
 }finally{await new Promise(r=>server.close(r));}
});
test('Wi-Fi bridge keeps pairing credentials in headers and maps device errors',async()=>{
 const originalFetch=globalThis.fetch, calls=[];
 globalThis.fetch=async(url,options)=>{
  if(String(url).startsWith('http://192.168.5.25')){calls.push({url,options});return new Response(JSON.stringify({board:'OSRCORE',protocol:1}),{status:200});}
  return originalFetch(url,options);
 };
 const server=http.createServer(handle);await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const base='http://127.0.0.1:'+server.address().port;
 try{
  const token='b'.repeat(32);
  const r=await originalFetch(base+'/api/lab/device',{method:'POST',headers:{'X-OSRCORE-Lab':'1'},body:JSON.stringify({action:'info',address:'192.168.5.25',token})});
  assert.equal(r.status,200);assert.equal((await r.json()).board,'OSRCORE');
  assert.equal(calls.length,1);assert.equal(calls[0].url,'http://192.168.5.25/api/info');assert.equal(calls[0].options.headers.Authorization,'Bearer '+token);assert.equal(calls[0].options.redirect,'error');
  const invalid=await originalFetch(base+'/api/lab/device',{method:'POST',headers:{'X-OSRCORE-Lab':'1'},body:JSON.stringify({action:'command',command:'drive on',address:'192.168.5.25',token})});
  assert.equal(invalid.status,400);assert.equal(calls.length,1);
 }finally{globalThis.fetch=originalFetch;await new Promise(r=>server.close(r));}
});
