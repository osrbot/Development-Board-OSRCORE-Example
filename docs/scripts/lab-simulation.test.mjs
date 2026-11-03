import test from 'node:test';
import assert from 'node:assert/strict';
import {lessons,initialValues,createState,lessonCode,stepSimulation} from '../.vitepress/theme/lab-simulation.mjs';
test('every lesson has finite outputs and valid trace lines at parameter limits',()=>{
 for(const lesson of lessons)for(const edge of ['value','min','max']){
  const values=Object.fromEntries(lesson.params.map(p=>[p.key,p[edge]]));const state=createState();
  for(let frame=0;frame<120;frame++){const out=stepSimulation(lesson.id,values,frame*50,.05,state);assert(Number.isFinite(out.value),`${lesson.id} ${edge}`);assert(out.line>=0&&out.line<lessonCode(lesson.id,values).length);}
 }
});
test('LED timing and zero brightness reflect the parameters',()=>{const v=initialValues(1);assert.equal(stepSimulation(1,v,1500,.05,createState()).value,0);assert.equal(stepSimulation(1,v,499,.05,createState()).label,'RED');assert.equal(stepSimulation(1,v,500,.05,createState()).label,'GREEN');assert.equal(stepSimulation(1,{...v,brightness:0},500,.05,createState()).color,'rgb(0,0,0)');});
test('PWM endpoints and receiver failsafe',()=>{assert.equal(stepSimulation(3,{pulse:1000},0,0,createState()).angle,-90);assert.equal(stepSimulation(3,{pulse:1500},0,0,createState()).angle,0);assert.equal(stepSimulation(4,{channel:1810,failsafe:1},0,0,createState()).value,1500);});
test('encoder speed uses configured counts per revolution',()=>{const state=createState();assert.equal(stepSimulation(6,{pps:1000,ppr:1000},1000,1,state).value,60);assert.equal(state.count,1000);});
test('motor feedback converges and robot failsafe clears motion immediately',()=>{const state=createState(),v=initialValues(9);for(let i=0;i<600;i++)stepSimulation(9,v,i*50,.05,state);assert(Math.abs(state.value-v.target)<.03);assert.equal(stepSimulation(12,{target:3,steering:30,failsafe:1},0,.05,{...createState(),value:2}).value,0);});
test('ideal gravity and heading correction remain consistent',()=>{assert.equal(stepSimulation(5,{angle:0},0,0,createState()).value,0);assert.equal(stepSimulation(11,{heading:135,offset:50},0,0,createState()).value,135);});
