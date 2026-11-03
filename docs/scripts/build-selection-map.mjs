import fs from 'node:fs/promises';
import crypto from 'node:crypto';
import * as T from 'three';
import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';
import {MeshoptDecoder} from 'meshoptimizer';
import {hardware} from '../.vitepress/theme/hardware-map.mjs';
const data=await fs.readFile('docs/public/hardware/osrcore-styled.glb');
const gltf=await new GLTFLoader().setMeshoptDecoder(MeshoptDecoder).parseAsync(data.buffer.slice(data.byteOffset,data.byteOffset+data.byteLength),'');
gltf.scene.scale.setScalar(1000);gltf.scene.updateMatrixWorld(true);
const meshes=[],components=[],chunks=[];
let offset=0;
gltf.scene.traverse(o=>{
 if(!o.isMesh)return;
 const pos=o.geometry.attributes.position,idx=o.geometry.index,at=i=>idx?idx.getX(i):i,ct=idx?idx.count:pos.count;
 const parent=new Int32Array(pos.count),canon=new Int32Array(pos.count),keys=new Map();
 for(let i=0;i<pos.count;i++){parent[i]=i;const k=[pos.getX(i),pos.getY(i),pos.getZ(i)].map(v=>v.toFixed(7)).join(',');if(!keys.has(k))keys.set(k,i);canon[i]=keys.get(k);}
 const root=i=>{while(parent[i]!==i){parent[i]=parent[parent[i]];i=parent[i]}return i};
 for(let i=0;i<ct;i+=3){const a=root(canon[at(i)]);parent[root(canon[at(i+1)])]=a;parent[root(canon[at(i+2)])]=a;}
 const groups=new Map(),v=new T.Vector3();
 for(let i=0;i<ct;i+=3){const r=root(canon[at(i)]);if(!groups.has(r))groups.set(r,{vertices:new Set(),box:new T.Box3(),triangles:0});const g=groups.get(r);g.triangles++;for(let j=0;j<3;j++)g.vertices.add(at(i+j));}
 const mask=new Uint8Array(pos.count),mesh=meshes.length;
 for(const g of groups.values()){
  for(const i of g.vertices){v.fromBufferAttribute(pos,i).applyMatrix4(o.matrixWorld);g.box.expandByPoint(v)}
  const c=g.box.getCenter(new T.Vector3()).toArray(),size=g.box.getSize(new T.Vector3()).toArray();
  components.push({mesh,material:o.material.name,c,size,min:g.box.min.toArray(),max:g.box.max.toArray(),triangles:g.triangles,vertices:[...g.vertices]});
 }
 meshes.push({name:o.name,material:o.material.name,count:pos.count,offset});offset+=mask.length;chunks.push(mask);
});
// Assignment operates on complete connected solids, never clips triangle surfaces.
// Millimetre extents calibrated to the CAD's connector housings, not page percentages.
const regions={
 'VIN-A':[[-17.51,22.54,4.4],[5.7,16,9.2],[1,2,3]],'VIN-B':[[-11.94,22.55,4.4],[5.7,16,9.2],[1,2,3]],
 'VOUT-A':[[11.92,22.54,4.4],[5.7,16,9.2],[1,2,3]],'VOUT-B':[[17.5,22.54,4.4],[5.7,16,9.2],[1,2,3]],
 'SW':[[-6.73,27.13,1.6],[4.8,4.8,4],[9,19,21,31]],'VO':[[6.74,27.13,1.6],[4.8,4.8,4],[9,19,21,31]],
 'EX1':[[-17.99,-12.83,2.4],[4.8,15.6,5.3],[32,33,34]],'LDS':[[-10.69,-12.57,2.22],[4,11,5.2],[9,19,21,31]],
 'EX3':[[9.11,-12.58,2.4],[4.6,10.7,5.2],[3,5,8,9]],'EX2':[[16.39,-7.39,4],[8,21.2,8.3],[1,3]],
 'USB1':[[-10.46,-27.37,2.4],[10.6,4.5,5.2],[3,5,8,9]],'USB2':[[-10.46,-21.97,2.4],[10.6,4.5,5.2],[3,5,8,9]],
 'USB3':[[10.45,-21.97,2.4],[10.6,4.5,5.2],[3,5,8,9]],'USBIN':[[10.45,-27.37,2.4],[10.6,4.5,5.2],[3,5,8,9]],
 'EX4':[[10.41,-27.8,-3.58],[10.7,4.8,4.1],[3,5,9,15,21]],'5V':[[-.01,-27.73,-3.48],[8.1,4.9,4.2],[3,5,9,15,21]],
 'ETH':[[-10.41,-27.8,-3.58],[10.7,4.8,4.1],[3,5,9,15,21]],'EX5':[[18.01,-21.58,-3.31],[4.8,6,3.6],[3,5,9,15,21]],
 'USBC':[[0,-26.05,1.1],[9.8,8.3,5],[3,4,5]],'CPU':[[1,5.31,1.5],[18.5,20,3.6],[3,11,26,27,28,29]],
 'IMU':[[-16.13,4.57,.47],[3,3.5,1.4],[1,3,36]],'MAG':[[-16,6.48,-2.1],[2,2.4,1.2],[42,43]],
 'BEEP':[[0,18.29,1],[5.7,5.8,2.5],[4,7,22]],'R':[[14.61,6.73,1],[3.4,4.4,2.5],[10,11,12]],'B':[[18.16,6.73,1],[3.4,4.4,2.5],[10,11,12]],
 'RGB':[[18.16,-22.73,.42],[2.5,2.5,1.3],[3,4,6]],'TF':[[-15.8,-12.8,-2.4],[8,12,2.5],[4,7,9,29]],
};
for(const g of components){
 const material=Number(g.material.replace('mtl',''));
 const candidates=hardware.map((h,i)=>{const [c,d,materials]=regions[h.id];if(!materials.includes(material)||g.size.some((v,k)=>v>d[k])||g.c.some((v,k)=>Math.abs(v-c[k])>d[k]/2))return null;return {i,score:g.c.reduce((s,v,k)=>s+((v-c[k])/d[k])**2,0)}}).filter(Boolean).sort((a,b)=>a.score-b.score);

 g.part=candidates.length?hardware[candidates[0].i].id:null;
 if(g.part)for(const v of g.vertices)chunks[g.mesh][v]=candidates[0].i+1;
 delete g.vertices;
}
await fs.writeFile('docs/public/hardware/selection-map.bin',Buffer.concat(chunks));
await fs.writeFile('docs/public/hardware/selection-map.json',JSON.stringify({modelHash:crypto.createHash('sha256').update(data).digest('hex'),ids:hardware.map(h=>h.id),meshes}));
await fs.writeFile('verification/selection-components.json',JSON.stringify(components,null,2));
console.log('Selection vertices',offset,'components',components.length);for(const h of hardware){const gs=components.filter(g=>g.part===h.id);console.log(h.id,gs.length,gs.reduce((s,g)=>s+g.triangles,0));}
