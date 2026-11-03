import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import assert from 'node:assert/strict';
const jobs=JSON.parse(await fs.readFile('verification/firmware-builds.json','utf8'));
assert.equal(jobs.length,13);
for(const job of jobs){
 assert.equal(job.state,'ready',job.example);
 assert(job.app.size<=0x200000,`${job.example}: application exceeds OTA slot`);
 for(const file of job.files){
  const bytes=await fs.readFile(path.join('.cache/lesson-builds',job.id,file.name));
  assert.equal(bytes.length,file.size);
  assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),file.sha256);
  const start=file.address,end=start+Math.ceil(file.size/4096)*4096;
  assert(end<=0x9000||start>=0xd000,`${job.example}: write overlaps NVS`);
  if(file.name===job.app.name)assert(bytes.includes(Buffer.from(job.id)),`${job.example}: missing embedded build ID`);
 }
 console.log(`${job.example}: image hashes, build ID, OTA size and preserved NVS verified`);
}
