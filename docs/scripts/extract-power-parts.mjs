import fs from 'node:fs/promises';
import * as T from 'three';
import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';
import {GLTFExporter} from 'three/addons/exporters/GLTFExporter.js';
import {MeshoptDecoder} from 'meshoptimizer';
globalThis.FileReader=class{readAsArrayBuffer(b){b.arrayBuffer().then(v=>{this.result=v;this.onloadend?.()})}};
const b=await fs.readFile('docs/public/hardware/osrcore-styled.glb');const gltf=await new GLTFLoader().setMeshoptDecoder(MeshoptDecoder).parseAsync(b.buffer.slice(b.byteOffset,b.byteOffset+b.byteLength),'');gltf.scene.scale.setScalar(1000);gltf.scene.updateMatrixWorld(true);
const parts=[['VIN-A',7.5],['VIN-B',21],['VOUT-A',79],['VOUT-B',93]].map(([id,x])=>({id,x:(x/100-.5)*41,y:24,group:new T.Group()}));
for(const p of parts)p.group.name=p.id;
gltf.scene.traverse(o=>{
 if(!o.isMesh||![1,2,3].includes(parseInt(o.material.name.replace('mtl',''))))return;
 const p=o.geometry.attributes.position,n=o.geometry.attributes.normal,idx=o.geometry.index,ct=idx?idx.count:p.count,parent=new Int32Array(p.count),canonical=new Int32Array(p.count),keys=new Map();
 for(let i=0;i<p.count;i++){const k=[p.getX(i),p.getY(i),p.getZ(i)].map(v=>v.toFixed(7)).join(',');if(!keys.has(k))keys.set(k,i);canonical[i]=keys.get(k);parent[i]=i;}
 const root=i=>{while(parent[i]!==i){parent[i]=parent[parent[i]];i=parent[i]}return i};
 const at=i=>idx?idx.getX(i):i;
 for(let i=0;i<ct;i+=3){const a=root(canonical[at(i)]);parent[root(canonical[at(i+1)])]=a;parent[root(canonical[at(i+2)])]=a;}
 const groups=new Map(),v=new T.Vector3();
 for(let i=0;i<ct;i+=3){const r=root(canonical[at(i)]);if(!groups.has(r))groups.set(r,{tris:[],box:new T.Box3()});const g=groups.get(r);g.tris.push(i);for(let j=0;j<3;j++){v.fromBufferAttribute(p,at(i+j)).applyMatrix4(o.matrixWorld);g.box.expandByPoint(v)}}
 const nm=new T.Matrix3().getNormalMatrix(o.matrixWorld);
 for(const g of groups.values()){
  const c=g.box.getCenter(new T.Vector3()),sz=g.box.getSize(new T.Vector3());if(sz.x>8||sz.y>16||sz.z>12||g.box.max.z<1)continue;
  for(const part of parts){if(Math.abs(c.x-part.x)>3.2||Math.abs(c.y-part.y)>7||c.z<0)continue;const positions=[],normals=[];for(const i of g.tris)for(let j=0;j<3;j++){const a=at(i+j);v.fromBufferAttribute(p,a).applyMatrix4(o.matrixWorld);positions.push(v.x-part.x,v.y-part.y,v.z);v.fromBufferAttribute(n,a).applyNormalMatrix(nm);normals.push(v.x,v.y,v.z)}const geo=new T.BufferGeometry();geo.setAttribute('position',new T.Float32BufferAttribute(positions,3));geo.setAttribute('normal',new T.Float32BufferAttribute(normals,3));part.group.add(new T.Mesh(geo,o.material.clone()));}
 }
});
const scene=new T.Group();scene.scale.setScalar(.001);
for(const p of parts){const box=new T.Box3().setFromObject(p.group);p.group.position.sub(box.getCenter(new T.Vector3()));scene.add(p.group);console.log(p.id,p.group.children.length,box.getSize(new T.Vector3()).toArray());}
const out=await new GLTFExporter().parseAsync(scene,{binary:true});await fs.writeFile('docs/public/hardware/power-parts.glb',Buffer.from(out));
