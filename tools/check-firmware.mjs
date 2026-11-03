import fs from 'node:fs/promises';
import {createBuild} from './lab-server.mjs';
const ids=process.argv.slice(2).map(Number);const jobs=[];
for(const lesson of ids.length?ids:Array.from({length:13},(_,i)=>i+1))jobs.push(await createBuild({lesson,parameters:{}}));
const finished=new Set();const timer=setInterval(async()=>{for(const job of jobs)if(['ready','error'].includes(job.state)&&!finished.has(job.id)){finished.add(job.id);console.log(job.example,job.state,job.id);if(job.state==='error')console.log(job.log.split('\n').filter(x=>/error:|FAILED:|Error/.test(x)).join('\n'));}if(finished.size===jobs.length){clearInterval(timer);await fs.mkdir('verification',{recursive:true});await fs.writeFile(`verification/firmware-builds${ids.length?'-'+ids.join('-'):''}.json`,JSON.stringify(jobs,null,2));process.exitCode=jobs.some(j=>j.state==='error')?1:0;}},1000);
