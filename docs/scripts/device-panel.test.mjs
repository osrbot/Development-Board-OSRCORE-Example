import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {createRequire} from 'node:module';
import {pathToFileURL} from 'node:url';
const require=createRequire(new URL('../package.json',import.meta.url));
const {parse,compileScript}=require('@vue/compiler-sfc');
const {createRenderer,reactive,nextTick,ref}=require('vue');

test('the device panel preserves connection across lessons and never guesses the running program',async()=>{
 const filename=new URL('../.vitepress/theme/DeviceLab.vue',import.meta.url);
 const {descriptor}=parse(await readFile(filename,'utf8'));
 let code=compileScript(descriptor,{id:'device-panel-test'}).content;
 // Run the component's real setup with Vue's renderer; stub only browser/platform adapters.
 code=code.replace("import {withBase} from 'vitepress';",'const withBase=x=>x;');
 code=code.replace("import {useLanguage} from './language.mjs';",'const useLanguage=()=>({en:{value:true},t:x=>x});');
 code=code.replace(/from (['"])([^'"]+)\1/g,(_,quote,source)=>{
  const url=source.startsWith('.')?new URL(source,filename).href:pathToFileURL(require.resolve(source)).href;
  return `from '${url}'`;
 });
 const {default:component}=await import('data:text/javascript;base64,'+Buffer.from(code).toString('base64'));
 component.render=()=>null;
 const saved=Object.fromEntries(['document','window','location','navigator','fetch'].map(k=>[k,Object.getOwnPropertyDescriptor(globalThis,k)]));
 let stream,opens=0,closes=0;const sent=[];
 const port={getInfo:()=>({usbVendorId:0x303a,usbProductId:0x1001}),async open(){opens++;this.readable=new ReadableStream({start(c){stream=c;}});this.writable=new WritableStream({write(bytes){sent.push(new TextDecoder().decode(bytes));}});},async close(){closes++;}};
 const globals={document:{addEventListener(){},removeEventListener(){},activeElement:null},window:{isSecureContext:true},location:{hostname:'test.github.io'},navigator:{serial:{getPorts:async()=>[port],requestPort:async()=>port}},fetch:async()=>{throw Error('offline fixture');}};
 for(const [key,value] of Object.entries(globals))Object.defineProperty(globalThis,key,{configurable:true,value});
 const renderer=createRenderer({createComment:()=>({}),insert(){},remove(){},parentNode(){},nextSibling(){}});
 const props=reactive({open:false,lesson:1,values:{}});
 const app=renderer.createApp(component,props);app.provide('osrcore-language',ref(true));
 try{
  app.mount({});const state=app._instance.setupState;
  await state.connectUSB();assert.equal(state.connected,true);assert.equal(state.device,null);
  await new Promise(r=>setTimeout(r,10));assert.ok(sent.includes('osrcore info\n'));
  state.lastFlash={lesson:1,example:'01_blink_led'};
  stream.enqueue(new TextEncoder().encode('OSRCORE_INFO {"board":"OSRCORE","protocol":1,"project":"07_nvs","version":"test"}\n'));
  await new Promise(r=>setTimeout(r,0));
  assert.equal(state.confirmedLesson,7);assert.equal(state.guideLesson,7);
  app._instance.props.lesson=2;await nextTick();
  assert.equal(state.confirmedLesson,7);assert.equal(state.guideLesson,7);assert.equal(opens,1);assert.equal(state.connected,true);
  stream.enqueue(new TextEncoder().encode('OSRCORE_INFO {"board":"OSRCORE","protocol":1,"project":"custom-app","version":"test"}\n'));
  await new Promise(r=>setTimeout(r,0));assert.equal(state.guideLesson,null);
  app._instance.props.lesson=1;app._instance.props.values={interval:100,brightness:80};await nextTick();
  stream.enqueue(new TextEncoder().encode('OSRCORE_INFO {"board":"OSRCORE","protocol":1,"project":"01_blink_led","version":"test","runtimeParameters":1,"parameters":{"interval":500,"brightness":32},"parametersSaved":false}\n'));
  await new Promise(r=>setTimeout(r,0));assert.equal(state.canApply,true);assert.equal(state.parametersMatch,false);
  const applying=state.applyParameters();await new Promise(r=>setTimeout(r,0));
  const request=JSON.parse(sent.at(-1).slice('osrcore params '.length));
  assert.equal(state.paramBusy,true);assert.equal(state.parametersMatch,false);
  app._instance.props.values={interval:200,brightness:80};await nextTick();
  stream.enqueue(new TextEncoder().encode('OSRCORE_PARAMS '+JSON.stringify({id:request.id,project:request.lesson,ok:true,values:request.values,saved:false})+'\n'));
  await applying;assert.equal(state.paramBusy,false);assert.equal(state.device.parameters.interval,100);
  assert.equal(state.parametersMatch,false); // Sliders changed in flight; never label their new values applied.
  const countBeforeSave=sent.length;await state.applyParameters(true);assert.equal(sent.length,countBeforeSave);
  await state.disconnect();assert.equal(state.device,null);assert.equal(state.connected,false);assert.ok(closes>0);
  assert.equal(state.guideLesson,1); // Last written lesson remains an unconfirmed guide, not a device identity.
 }finally{app.unmount();for(const [key,value] of Object.entries(saved)){if(value)Object.defineProperty(globalThis,key,value);else delete globalThis[key];}}
});
