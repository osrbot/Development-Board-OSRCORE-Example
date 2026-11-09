// Shared validation for published images and browser downloads.
export function validateManifest(data,example){
 if(data?.schema!==1||typeof data.revision!=='string'||!data.revision||data.chip!=='ESP32-S3'||data.example!==example||!Array.isArray(data.files)||data.files.length!==4)throw Error('Invalid course firmware manifest');
 const limits=new Map([[0,0x8000],[0x8000,0x1000],[0xd000,0x2000],[0x10000,0x200000]]),seen=new Set();
 for(const f of data.files){
  if(!limits.has(f.address)||seen.has(f.address)||!Number.isInteger(f.size)||f.size<1||f.size>limits.get(f.address)||!/^[-\w.]+\.bin$/.test(f.name)||!/^\w{64}$/.test(f.sha256)||!/^\w{32}$/.test(f.md5))throw Error('Invalid firmware image or unsafe flash range');
  if(!/^[a-f0-9]+$/.test(f.sha256+f.md5))throw Error('Invalid image checksum');
  seen.add(f.address);
 }
 return data;
}
export async function fetchCourseFirmware(base,example,fetcher=fetch){
 const folder=base+example+'/';
 const response=await fetcher(folder+'manifest.json');
 if(!response.ok)throw Error('Course firmware is unavailable. Please retry.');
 const manifest=validateManifest(await response.json(),example);
 return {...manifest,state:'ready',preset:true,files:manifest.files.map(f=>({...f,url:folder+f.name}))};
}
export async function fetchFlashFiles(files,fetcher=fetch){
 const result=[];
 for(const f of files){
  const response=await fetcher(f.url);
  if(!response.ok)throw Error('Firmware download failed');
  const bytes=new Uint8Array(await response.arrayBuffer());
  const digest=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)),x=>x.toString(16).padStart(2,'0')).join('');
  if(bytes.length!==f.size||digest!==f.sha256)throw Error('Firmware checksum mismatch');
  let data='';for(let i=0;i<bytes.length;i+=8192)data+=String.fromCharCode(...bytes.subarray(i,i+8192));
  result.push({address:f.address,data});
 }
 return result;
}
