import test from 'node:test';
import assert from 'node:assert/strict';
import {resetS3ToApplication,previouslySelectedPort,reconnectAuthorizedUSB} from '../.vitepress/theme/usb-session.mjs';
const port=(pid=0x1001)=>({getInfo:()=>({usbVendorId:0x303a,usbProductId:pid})});
test('S3 application reset clears forced download and pulses RTS with BOOT released',async()=>{
 const calls=[];
 await resetS3ToApplication({writeReg:async(...a)=>calls.push(['reg',...a])},{setDTR:async v=>calls.push(['dtr',v]),setRTS:async v=>calls.push(['rts',v])},async ms=>calls.push(['wait',ms]));
 assert.deepEqual(calls,[['reg',0x6000812c,0,1],['dtr',false],['rts',true],['wait',200],['rts',false],['wait',250]]);
});
test('reuse selected device; never select an ambiguous or different device',()=>{
 const original=port(),replacement=port();
 assert.equal(previouslySelectedPort([original,replacement],original),original);
 assert.equal(previouslySelectedPort([replacement],original),replacement);
 assert.equal(previouslySelectedPort([replacement,port()],original),null);
 assert.equal(previouslySelectedPort([port(2)],original),null);
 assert.equal(previouslySelectedPort([replacement],null),null);
 assert.equal(previouslySelectedPort([replacement],original,[replacement]),null);
});
test('wait for re-enumeration and retry port opening without asking for permission',async()=>{
 const original=port(),replacement=port();let polls=0,opens=0,cleaned=0;
 const result=await reconnectAuthorizedUSB({serial:{getPorts:async()=>++polls<3?[]:[replacement]},previous:original,open:async p=>{assert.equal(p,replacement);if(++opens===1)throw Error('not ready');},cleanup:async()=>cleaned++,wait:async()=>{},attempts:5});
 assert.equal(result,true);assert.equal(polls,4);assert.equal(cleaned,1);
});
test('missing/ambiguous devices time out; disposal cancels retries',async()=>{
 let opened=0;const options={serial:{getPorts:async()=>[port(),port()]},previous:port(),open:async()=>opened++,cleanup:async()=>{},wait:async()=>{},attempts:2};
 assert.equal(await reconnectAuthorizedUSB(options),false);assert.equal(opened,0);
 assert.equal(await reconnectAuthorizedUSB({...options,cancelled:()=>true}),false);
});

test('flasher cleanup releases an actual ReadableStream lock before closing the port',async()=>{
 const {closeFlashTransport}=await import('../.vitepress/theme/usb-session.mjs');
 const readable=new ReadableStream();const reader=readable.getReader();let closed=false;
 const transport={reader,device:{readable,writable:new WritableStream(),close:async()=>{assert.equal(readable.locked,false);closed=true;}}};
 assert.equal(readable.locked,true);
 await closeFlashTransport(transport);
 assert.equal(closed,true);assert.equal(transport.reader,undefined);
});
test('a stalled stream cancellation cannot block releasing its lock',async()=>{
 const {closeFlashTransport}=await import('../.vitepress/theme/usb-session.mjs');
 const readable=new ReadableStream({cancel:()=>new Promise(()=>{})});const reader=readable.getReader();let closed=false;
 await closeFlashTransport({reader,device:{readable,close:async()=>{closed=true;}}},20);
 assert.equal(readable.locked,false);assert.equal(closed,true);
});
test('port close and enumeration have bounded failure paths',async()=>{
 const {closeFlashTransport}=await import('../.vitepress/theme/usb-session.mjs');
 await assert.rejects(closeFlashTransport({device:{readable:new ReadableStream(),close:()=>new Promise(()=>{})}},20),/close timed out/);
 const result=await reconnectAuthorizedUSB({serial:{getPorts:()=>new Promise(()=>{})},previous:port(),open:async()=>{},cleanup:async()=>{},timeoutMs:20});
 assert.equal(result,false);
});
