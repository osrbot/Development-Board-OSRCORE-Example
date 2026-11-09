import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {createRequire} from 'node:module';
import {pathToFileURL} from 'node:url';
import {lessons,lessonCode,initialValues} from '../.vitepress/theme/lab-simulation.mjs';
const require=createRequire(new URL('../package.json',import.meta.url));
const {parse,compileScript}=require('@vue/compiler-sfc');
const {createRenderer,nextTick}=require('vue');

test('header selection follows the lesson GPIO and preserves manual selection while running',async()=>{
 const filename=new URL('../.vitepress/theme/PartInspector.vue',import.meta.url);
 const {descriptor}=parse(await readFile(filename,'utf8'));
 let code=compileScript(descriptor,{id:'pin-selection-test'}).content
  .replace("import {useLanguage} from './language.mjs';",'const useLanguage=()=>({en:{value:true},t:x=>x});')
  .replace("import ConnectorDiagram from './ConnectorDiagram.vue';",'const ConnectorDiagram={};');
 code=code.replace(/from (['"])([^'"]+)\1/g,(_,quote,source)=>`from '${source.startsWith('.')?new URL(source,filename).href:pathToFileURL(require.resolve(source)).href}'`);
 const {default:component}=await import('data:text/javascript;base64,'+Buffer.from(code).toString('base64'));
 component.render=()=>null;
 const renderer=createRenderer({createComment:()=>({}),insert(){},remove(){},parentNode(){},nextSibling(){}});
 const app=renderer.createApp(component,{id:'EX2',preferredGpio:2,running:false});
 try {
  app.mount({});const state=app._instance.setupState;
  assert.equal(state.pin,'8L');assert.equal(state.chosen.gpio,2);
  state.pin='8M';await nextTick();assert.equal(state.chosen.net,'VCC_ESC');
  app._instance.props.running=true;await nextTick();assert.equal(state.pin,'8M');
  app._instance.props.preferredGpio=44;await nextTick();assert.equal(state.pin,'4L');
  app._instance.props.id='EX1';app._instance.props.preferredGpio=undefined;await nextTick();
  assert.equal(state.chosen.gpio,17);
 } finally {app.unmount();}
});

test('every displayed lesson code excerpt uses English comments',async()=>{
 for(const lesson of lessons){
  const code=lessonCode(lesson.id,initialValues(lesson.id)).join('\n');
  assert.doesNotMatch(code,/\p{Script=Han}/u,`Lesson ${lesson.id} contains untranslated code`);
 }
 for(const file of ['../tutorial.md','../zh/tutorial.md']){
  const tutorial=await readFile(new URL(file,import.meta.url),'utf8');
  for(const [block] of tutorial.matchAll(/```c\n[\s\S]*?```/g))assert.doesNotMatch(block,/\p{Script=Han}/u,`${file} contains untranslated C code`);
 }
});
