import {validateFirmwareRequest} from './lesson-contract.mjs';
export function sameParameters(a,b){
 if(!a||!b)return false;
 const keys=Object.keys(a);
 return keys.length===Object.keys(b).length&&keys.every(k=>typeof b[k]==='number'&&Number.isFinite(b[k])&&Math.abs(a[k]-b[k])<1e-6);
}
// Only a matching device response completes a request. Writing USB bytes is not success.
export function createParameterChannel(send,{timeoutMs=8000}={}){
 let pending=null,sequence=0;
 function finish(error,reply){if(!pending)return;const request=pending;pending=null;clearTimeout(request.timer);error?request.reject(error):request.resolve(reply);}
 return {
  request(lesson,values,save=false){
   if(pending)return Promise.reject(Error('request_busy'));
   let checked;try{checked=validateFirmwareRequest(lesson,values);if(!Object.keys(checked.parameters).length)throw Error('no_runtime_parameters');}catch(e){return Promise.reject(e);}
   const id=`${Date.now().toString(36)}-${++sequence}`;
   return new Promise((resolve,reject)=>{
    pending={id,project:checked.example,values:checked.parameters,save,resolve,reject,timer:setTimeout(()=>finish(Error('response_timeout')),timeoutMs)};
    Promise.resolve().then(()=>send('osrcore params '+JSON.stringify({id,lesson:checked.example,values:checked.parameters,save}))).catch(e=>{if(pending?.id===id)finish(e);});
   });
  },
  receive(line){
   if(!line.startsWith('OSRCORE_PARAMS '))return false;
   let reply;try{reply=JSON.parse(line.slice(15));}catch{return true;}
   if(!pending||reply.id!==pending.id)return false;
   if(reply.project!==pending.project){finish(Error('lesson_mismatch'));return true;}
   if(reply.ok!==true){finish(Error(reply.error||'device_rejected'));return true;}
   if(!sameParameters(pending.values,reply.values)||(pending.save&&reply.saved!==true)){finish(Error('response_mismatch'));return true;}
   finish(null,reply);return true;
  },
  cancel(){finish(Error('device_disconnected'));},
 };
}
