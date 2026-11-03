import {test} from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import crypto from 'node:crypto';
import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';
import {MeshoptDecoder} from 'meshoptimizer';
import {hardware} from '../.vitepress/theme/hardware-map.mjs';
const base=new URL('../public/hardware/',import.meta.url);
const [model,mask,map]=await Promise.all([fs.readFile(new URL('osrcore-styled.glb',base)),fs.readFile(new URL('selection-map.bin',base)),fs.readFile(new URL('selection-map.json',base),'utf8').then(JSON.parse)]);
const gltf=await new GLTFLoader().setMeshoptDecoder(MeshoptDecoder).parseAsync(model.buffer.slice(model.byteOffset,model.byteOffset+model.byteLength),'');
test('selection mask matches model and every hardware item has selectable geometry',()=>{
 assert.equal(crypto.createHash('sha256').update(model).digest('hex'),map.modelHash);
 assert.deepEqual(map.ids,hardware.map(h=>h.id));
 assert.equal(map.meshes.reduce((n,m)=>n+m.count,0),mask.length);
 const owners=new Set(mask);for(let i=1;i<=map.ids.length;i++)assert.ok(owners.has(i),map.ids[i-1]);
});
test('each triangle has a single owner; PCB surfaces are never selectable',()=>{
 let meshIndex=0;
 gltf.scene.traverse(o=>{if(!o.isMesh)return;const m=map.meshes[meshIndex++],p=o.geometry.attributes.position,ix=o.geometry.index,at=i=>ix?ix.getX(i):i;
 assert.equal(p.count,m.count);assert.equal(o.material.name,m.material);
 for(let i=0;i<(ix?ix.count:p.count);i+=3){const id=mask[m.offset+at(i)];assert.ok(id<=map.ids.length);assert.equal(mask[m.offset+at(i+1)],id);assert.equal(mask[m.offset+at(i+2)],id)}
 if(['mtl52','mtl53'].includes(m.material))assert.ok(mask.subarray(m.offset,m.offset+m.count).every(id=>id===0));
 });
});

test('EX2 physical pin coordinates cover exactly 24 independent pins',async()=>{
 const {headerPinPosition,headerPinNumberAt}=await import('../.vitepress/theme/header-pins.mjs');
 const pins=hardware.find(h=>h.id==='EX2').pins;
 assert.deepEqual(headerPinPosition(pins[0]),[13.847,1.501,8.5]);
 assert.equal(headerPinNumberAt(...headerPinPosition(pins[23])),24);
 assert.equal(headerPinNumberAt(16.387,1.501,1),0);
 gltf.scene.scale.setScalar(1000);gltf.scene.updateMatrixWorld(true);
 const {Vector3}=await import('three');const found=new Set();let mi=0;
 gltf.scene.traverse(o=>{if(!o.isMesh)return;const m=map.meshes[mi++];if(o.material.name!=='mtl3')return;const p=o.geometry.attributes.position,v=new Vector3();for(let i=0;i<p.count;i++){if(map.ids[mask[m.offset+i]-1]!=='EX2')continue;v.fromBufferAttribute(p,i).applyMatrix4(o.matrixWorld);const id=headerPinNumberAt(v.x,v.y,v.z);if(id)found.add(id)}});
 assert.deepEqual([...found].sort((a,b)=>a-b),Array.from({length:24},(_,i)=>i+1));
});
