import http from 'node:http';
import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import crypto from 'node:crypto';
import {spawn} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {validateFirmwareRequest,configHeader,exampleDirs} from '../docs/.vitepress/theme/lesson-contract.mjs';
export const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const buildRoot=path.join(root,'.cache/lesson-builds'),dist=path.join(root,'docs/.vitepress/dist');
const jobs=new Map();let queue=Promise.resolve();let pending=0;
export function privateAddress(host){return /^(10\.(\d{1,3}\.){2}\d{1,3}|192\.168\.\d{1,3}\.\d{1,3}|172\.(1[6-9]|2\d|3[01])\.\d{1,3}\.\d{1,3})$/.test(host)&&host.split('.').every(n=>Number(n)<=255);}
export function validateDevice({address,token}){if(!privateAddress(address)||!/^\w{32}$/.test(token)||!/^[a-f0-9]{32}$/.test(token))throw Error('请输入板子在局域网中的 IPv4 地址与 USB 配对码');return {address,token};}
async function sourceFingerprint(example){const hash=crypto.createHash('sha256');async function walk(dir){for(const entry of (await fs.readdir(dir,{withFileTypes:true})).sort((a,b)=>a.name.localeCompare(b.name))){if(['build','sdkconfig','sdkconfig.old','dependencies.lock','managed_components'].includes(entry.name))continue;const p=path.join(dir,entry.name);if(entry.isDirectory())await walk(p);else{hash.update(path.relative(root,p));hash.update(await fs.readFile(p));}}}await walk(path.join(root,example));await walk(path.join(root,'components'));hash.update(await fs.readFile(path.join(root,'partitions_ota.csv')));return hash.digest('hex');}
function run(command,args,cwd,job){return new Promise((resolve,reject)=>{const child=spawn(command,args,{cwd,env:process.env});const timer=setTimeout(()=>child.kill('SIGTERM'),20*60*1000);const append=chunk=>{job.log=(job.log+chunk.toString()).slice(-16000);};child.stdout.on('data',append);child.stderr.on('data',append);child.on('error',e=>{clearTimeout(timer);reject(e)});child.on('close',code=>{clearTimeout(timer);code===0?resolve():reject(Error(`ESP-IDF 编译失败（${code}），请查看构建日志`));});});}
export async function createBuild(input){
 const request=validateFirmwareRequest(input.lesson,input.parameters),sourceHash=await sourceFingerprint(request.example);
 const id=crypto.createHash('sha256').update(JSON.stringify({request,sourceHash,toolchain:'idf6.0.1'})).digest('hex').slice(0,24);
 if(jobs.has(id)&&jobs.get(id).state!=='error')return jobs.get(id);
 const dir=path.join(buildRoot,id);try{const cached=JSON.parse(await fs.readFile(path.join(dir,'result.json'),'utf8'));jobs.set(id,cached);return cached;}catch{}
 if(jobs.has(id)&&jobs.get(id).state!=='error')return jobs.get(id);
 if(pending>=14)throw Error('构建队列已满，请稍后重试');
 const job={id,...request,sourceHash,state:'queued',log:'',files:[],createdAt:new Date().toISOString()};jobs.set(id,job);pending++;
 queue=queue.catch(()=>{}).then(async()=>{job.state='building';try{
  if(await sourceFingerprint(request.example)!==sourceHash)throw Error('源码在排队期间有更新，请重新编译');
  await fs.mkdir(dir,{recursive:true});const project=path.join(dir,request.example);
  await fs.cp(path.join(root,request.example),project,{recursive:true,filter:p=>!['build','sdkconfig','sdkconfig.old','managed_components','dependencies.lock'].includes(path.basename(p))});
  await fs.cp(path.join(root,'components'),path.join(dir,'components'),{recursive:true});await fs.copyFile(path.join(root,'partitions_ota.csv'),path.join(dir,'partitions_ota.csv'));
  if(await sourceFingerprint(request.example)!==sourceHash)throw Error('源码在准备期间有更新，请重新编译');
  const header=path.join(dir,'lesson-config.h');await fs.writeFile(header,configHeader(request.lesson,request.parameters));
  const idf=process.env.OSRCORE_IDF_PATH||path.join(os.homedir(),'esp/esp-idf');await fs.access(path.join(idf,'export.sh'));
  await run('/bin/zsh',['-c','source "$1/export.sh" >/dev/null 2>&1 && idf.py -C "$2" -D "OSRCORE_LAB_CONFIG=$3" -D "PROJECT_VER=$4" build','osrcore-build',idf,project,header,id],root,job);
  const output=path.join(project,'build'),flash=JSON.parse(await fs.readFile(path.join(output,'flasher_args.json'),'utf8'));
  for(const [address,file] of Object.entries(flash.flash_files)){const bytes=await fs.readFile(path.resolve(output,file));const name=path.basename(file);await fs.writeFile(path.join(dir,name),bytes);job.files.push({name,address:Number(address),size:bytes.length,md5:crypto.createHash('md5').update(bytes).digest('hex'),sha256:crypto.createHash('sha256').update(bytes).digest('hex'),url:`/api/lab/builds/${id}/files/${name}`});}
  job.app=job.files.find(f=>f.name===request.example+'.bin');if(!job.app)throw Error('构建未生成应用固件');job.state='ready';job.log=job.log.slice(-6000);await fs.writeFile(path.join(dir,'result.json'),JSON.stringify(job));
 }catch(e){job.state='error';job.error=e.message;}finally{pending--;}});
 return job;
}
async function getBuild(id){if(!/^[a-f0-9]{24}$/.test(id))throw Error('无效构建编号');const job=jobs.get(id)||JSON.parse(await fs.readFile(path.join(buildRoot,id,'result.json'),'utf8'));if(job.state==='ready')for(const file of job.files)if(!file.md5)file.md5=crypto.createHash('md5').update(await fs.readFile(path.join(buildRoot,id,file.name))).digest('hex');jobs.set(id,job);return job;}
async function body(req){let data='';for await(const chunk of req){data+=chunk;if(data.length>8192)throw Error('请求过大');}return JSON.parse(data||'{}');}
function send(res,status,data){res.writeHead(status,{'Content-Type':'application/json','Cache-Control':'no-store'});res.end(JSON.stringify(data));}
export async function handle(req,res){try{
 const host=req.headers.host||'',url=new URL(req.url,'http://'+host);
 const allowedHosts=new Set(['localhost','127.0.0.1',...Object.values(os.networkInterfaces()).flat().filter(Boolean).map(x=>x.address)]);
 if(!allowedHosts.has(url.hostname)){send(res,403,{error:'Host not allowed'});return;}
 if(url.pathname.startsWith('/api/lab/')){
  if(req.method==='POST'&&(req.headers['x-osrcore-lab']!=='1'||(req.headers.origin&&req.headers.origin!==url.origin))){send(res,403,{error:'请从本地课程页面发起操作'});return;}
  if(url.pathname==='/api/lab/health'){send(res,200,{local:true,compiler:'ESP-IDF',pending,examples:exampleDirs.length-1});return;}
  if(url.pathname==='/api/lab/builds'&&req.method==='POST'){send(res,202,await createBuild(await body(req)));return;}
  const match=url.pathname.match(/^\/api\/lab\/builds\/([a-f0-9]{24})(?:\/files\/([\w.-]+))?$/);
  if(match){const job=await getBuild(match[1]);if(!match[2]){send(res,200,job);return;}const file=job.files.find(f=>f.name===match[2]);if(job.state!=='ready'||!file)throw Error('固件尚未准备好');const bytes=await fs.readFile(path.join(buildRoot,job.id,file.name));res.writeHead(200,{'Content-Type':'application/octet-stream','Content-Length':bytes.length,'Cache-Control':'no-store'});res.end(bytes);return;}
  if(url.pathname==='/api/lab/device'&&req.method==='POST'){
   const input=await body(req),{address,token}=validateDevice(input);let route='/api/info',method='GET',payload,headers={Authorization:'Bearer '+token};
   if(input.action==='logs'){route='/api/logs?after='+Math.max(0,Math.floor(Number(input.after)||0));}
   else if(input.action==='flash'){const job=await getBuild(input.build);if(job.state!=='ready'||!job.app)throw Error('固件未编译完成');route='/api/ota';method='POST';payload=await fs.readFile(path.join(buildRoot,job.id,job.app.name));headers['Content-Type']='application/octet-stream';headers['X-Firmware-SHA256']=job.app.sha256;}
   else if(input.action==='command'){if(!['save','reboot'].includes(input.command))throw Error('不支持的设备命令');route='/api/command';method='POST';payload=input.command;}
   else if(input.action!=='info')throw Error('不支持的设备操作');
   const response=await fetch(`http://${address}${route}`,{method,headers,body:payload,signal:AbortSignal.timeout(input.action==='flash'?120000:6000),redirect:'error'});
   const text=await response.text();if(!response.ok)throw Error(`开发板响应 ${response.status}: ${text.slice(0,150)}`);send(res,200,JSON.parse(text));return;
  }
  send(res,404,{error:'接口不存在'});return;
 }
 if(req.method!=='GET'&&req.method!=='HEAD'){send(res,405,{error:'Method not allowed'});return;}
 let name=decodeURIComponent(url.pathname);if(name.endsWith('/'))name+='index.html';const file=path.resolve(dist,'.'+name);if(!file.startsWith(dist+path.sep))throw Error('Invalid path');let bytes,actual=file;try{bytes=await fs.readFile(file);}catch{actual=file+'.html';try{bytes=await fs.readFile(actual);}catch{send(res,404,{error:'页面不存在'});return;}}
 const types={'.html':'text/html; charset=utf-8','.js':'text/javascript','.css':'text/css','.json':'application/json','.svg':'image/svg+xml','.png':'image/png','.jpg':'image/jpeg','.woff2':'font/woff2','.glb':'model/gltf-binary','.pdf':'application/pdf'};
 res.writeHead(200,{'Content-Type':types[path.extname(actual)]||'application/octet-stream','Cache-Control':'no-cache'});res.end(req.method==='HEAD'?undefined:bytes);
 }catch(e){send(res,400,{error:e.message});}}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url)){const port=Number(process.env.PORT||5173);http.createServer(handle).listen(port,'0.0.0.0',()=>console.log(`OSRCORE local lab: http://localhost:${port}`));}
