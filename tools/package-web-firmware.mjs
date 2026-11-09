import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import {exampleDirs} from '../docs/.vitepress/theme/lesson-contract.mjs';
import {validateManifest} from '../docs/.vitepress/theme/firmware-download.mjs';
const [example,output='docs/public/firmware']=process.argv.slice(2);
if(!exampleDirs.slice(1).includes(example))throw Error('Unknown example');
const build=path.resolve(example,'build'),dest=path.resolve(output,example);
const args=JSON.parse(await fs.readFile(path.join(build,'flasher_args.json'),'utf8'));
await fs.mkdir(dest,{recursive:true});
const files=[];
for(const [offset,file] of Object.entries(args.flash_files)){
 const bytes=await fs.readFile(path.resolve(build,file)),name=path.basename(file);
 files.push({name,address:Number(offset),size:bytes.length,sha256:crypto.createHash('sha256').update(bytes).digest('hex'),md5:crypto.createHash('md5').update(bytes).digest('hex')});
 await fs.writeFile(path.join(dest,name),bytes);
}
const manifest={schema:1,chip:'ESP32-S3',example,revision:process.env.GITHUB_SHA||'local',files};
validateManifest(manifest,example);
await fs.writeFile(path.join(dest,'manifest.json'),JSON.stringify(manifest,null,2)+'\n');
console.log(example+': packaged separate images, hashes and NVS-safe write ranges');
