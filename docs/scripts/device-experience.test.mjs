import test from 'node:test';
import assert from 'node:assert/strict';
import {parseDeviceInfo,deviceProblem,physicalLessons} from '../.vitepress/theme/device-experience.mjs';

test('only a structured OSRCORE reply confirms the running lesson',()=>{
 const reply='OSRCORE_INFO '+JSON.stringify({board:'OSRCORE',protocol:1,project:'07_nvs',version:'release-1'});
 assert.deepEqual(parseDeviceInfo(reply),{board:'OSRCORE',protocol:1,project:'07_nvs',version:'release-1',lesson:7});
 for(const line of ['USB connected','Download complete','STATUS fw_update=1','> '+reply,'OSRCORE_INFO {','OSRCORE_INFO {"board":"other","protocol":1,"project":"07_nvs","version":"x"}'])assert.equal(parseDeviceInfo(line),null);
 const unknown=parseDeviceInfo(reply.replace('07_nvs','custom-app'));
 assert.equal(unknown.lesson,-1); // Do not attach another lesson's controls to a custom program.
});
test('actionable errors distinguish download, verification, browser and device failures',()=>{
 assert.equal(deviceProblem('SHA256 checksum mismatch'),'verification');
 assert.equal(deviceProblem('开发板 Flash 校验失败，请重新下载'),'verification');
 assert.equal(deviceProblem('Failed to fetch'),'firmware');
 assert.equal(deviceProblem('USB stream remains locked'),'occupied');
 assert.equal(deviceProblem('Access denied'),'permission');
 assert.equal(deviceProblem('仅支持 ESP32-S3，已停止写入'),'chip');
 assert.equal(deviceProblem('Failed to connect: no serial data'),'connection');
});
test('all downloadable lessons have paired physical actions and observations',()=>{
 assert.equal(physicalLessons.length,14);
 for(const guide of physicalLessons.slice(1))for(const lang of ['en','zh']){
  assert.equal(guide[lang].length,2);
  assert.ok(guide[lang].every(text=>typeof text==='string'&&text.length>0));
 }
});
