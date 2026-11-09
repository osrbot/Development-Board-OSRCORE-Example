import test from 'node:test';
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {validateManifest,fetchCourseFirmware,fetchFlashFiles} from '../.vitepress/theme/firmware-download.mjs';
const files=[0,0x8000,0xd000,0x10000].map((address,i)=>({address,name:`part-${i}.bin`,size:16,sha256:'a'.repeat(64),md5:'b'.repeat(32)}));
const manifest=()=>({schema:1,revision:'test',chip:'ESP32-S3',example:'01_blink_led',files:structuredClone(files)});
test('reject wrong controller, wrong lesson and writes over NVS or OTA capacity',()=>{
 assert.equal(validateManifest(manifest(),'01_blink_led').files.length,4);
 for(const modify of [m=>m.chip='ESP32',m=>m.example='02_buzzer',m=>m.files[1].size=0x2000,m=>m.files[3].size=0x200001,m=>m.files[2].address=0x9000,m=>m.files[1].address=0,m=>m.files[0].name='../bad.bin',m=>m.files[0].sha256='z'.repeat(64)]){const m=manifest();modify(m);assert.throws(()=>validateManifest(m,'01_blink_led'));}
});
test('published downloads retain Pages base and never use local build APIs',async()=>{
 const requests=[];const m=await fetchCourseFirmware('/osrcore-learn/firmware/','01_blink_led',async url=>{requests.push(url);return {ok:true,json:async()=>manifest()};});
 assert.deepEqual(requests,['/osrcore-learn/firmware/01_blink_led/manifest.json']);
 assert.equal(m.files[0].url,'/osrcore-learn/firmware/01_blink_led/part-0.bin');
 await assert.rejects(fetchCourseFirmware('/firmware/','01_blink_led',async()=>({ok:false})),/unavailable/);
});
test('verify actual payload hash and length before returning any flash data',async()=>{
 const bytes=Buffer.from([0xe9,0,1,255]);const f={url:'/app.bin',address:0x10000,size:bytes.length,sha256:createHash('sha256').update(bytes).digest('hex')};
 const fetcher=async()=>({ok:true,arrayBuffer:async()=>bytes});
 assert.equal((await fetchFlashFiles([f],fetcher))[0].data.charCodeAt(3),255);
 await assert.rejects(fetchFlashFiles([{...f,size:5}],fetcher),/checksum/);
 await assert.rejects(fetchFlashFiles([{...f,sha256:'a'.repeat(64)}],fetcher),/checksum/);
});
