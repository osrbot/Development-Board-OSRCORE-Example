// Convert the user's EasyEDA OBJ export, including silk geometry, to a web PBR GLB.
import fs from 'node:fs/promises';
import path from 'node:path';
import * as THREE from 'three';
import { OBJLoader } from 'three/addons/loaders/OBJLoader.js';
import { MTLLoader } from 'three/addons/loaders/MTLLoader.js';
import { GLTFExporter } from 'three/addons/exporters/GLTFExporter.js';
const [input,output] = process.argv.slice(2);
if (!input || !output) throw new Error('Usage: node convert-eda-obj.mjs input.obj output.glb');
globalThis.FileReader = class {
 readAsArrayBuffer(blob) { blob.arrayBuffer().then(value=>{this.result=value; this.onloadend?.();}); }
 readAsDataURL(blob) { blob.arrayBuffer().then(value=>{this.result='data:'+blob.type+';base64,'+Buffer.from(value).toString('base64');this.onloadend?.();}); }
};
const materialText=await fs.readFile(input.replace(/\.obj$/i,'.mtl'),'utf8');
const materials=new MTLLoader().parse(materialText,path.dirname(input)+'/');
materials.preload();
const model=new OBJLoader().setMaterials(materials).parse(await fs.readFile(input,'utf8'));
const converted=new Map();
let triangles=0;
model.traverse(mesh=>{
 if(!mesh.isMesh)return;
 triangles += (mesh.geometry.index?.count || mesh.geometry.attributes.position.count)/3;
 const convert=(m)=>{
  if(converted.has(m.name))return converted.get(m.name);
  const c=m.color;
  const neutral=Math.max(c.r,c.g,c.b)-Math.min(c.r,c.g,c.b)<0.03;
  const bright=(c.r+c.g+c.b)/3;
  const metal=(neutral && bright>.22 && bright<.94) || (c.r>.4 && c.g>.2 && c.b<.15);
  const next=new THREE.MeshStandardMaterial({name:m.name,color:c,metalness:metal?.75:0,roughness:metal?.4:.72,side:THREE.DoubleSide});
  if(m.name==='mtl1'){next.color.set('#191c20');next.metalness=0;next.roughness=.65;}
  converted.set(m.name,next);return next;
 };
 mesh.material=Array.isArray(mesh.material)?mesh.material.map(convert):convert(mesh.material);
});
const box=new THREE.Box3().setFromObject(model);
console.log({bounds:{min:box.min.toArray(),max:box.max.toArray()},triangles,materials:converted.size});
model.scale.setScalar(.001);
const result=await new GLTFExporter().parseAsync(model,{binary:true,onlyVisible:true});
await fs.writeFile(output,Buffer.from(result));
console.log({output,bytes:result.byteLength});
