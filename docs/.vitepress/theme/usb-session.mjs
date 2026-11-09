const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));

// ESP32-S3 force-download workaround from Espressif's esp32s3.py hard_reset.
// https://github.com/espressif/esptool/blob/master/esptool/targets/esp32s3.py
export async function resetS3ToApplication(loader,transport,wait=pause){
 await withDeadline(()=>loader.writeReg(0x6000812c,0,1),3000,'Reset command timed out');
 await withDeadline(()=>transport.setDTR(false),2000,'USB DTR timed out');
 await withDeadline(()=>transport.setRTS(true),2000,'USB reset assertion timed out');
 await wait(200);
 await withDeadline(()=>transport.setRTS(false),2000,'USB reset release timed out');
 await wait(250);
}

// Web Serial exposes VID/PID, not a stable serial number. Never choose between
// multiple matching devices after re-enumeration; ask the user in that case.
export function previouslySelectedPort(ports,previous,excluded=[]){
 if(ports.includes(previous))return previous;
 const info=previous?.getInfo();
 if(info?.usbVendorId==null||info?.usbProductId==null)return null;
 const matches=ports.filter(p=>{if(excluded.includes(p))return false;const candidate=p.getInfo();return candidate.usbVendorId===info.usbVendorId&&candidate.usbProductId===info.usbProductId;});
 return matches.length===1?matches[0]:null;
}
export async function reconnectAuthorizedUSB({serial,previous,open,cleanup,wait=pause,cancelled=()=>false,attempts=30,excluded=[],timeoutMs=15000}){
 const end=Date.now()+timeoutMs;
 for(let i=0;i<attempts&&Date.now()<end;i++){
  if(cancelled())return false;
  let ports;try{ports=await withDeadline(()=>serial.getPorts(),Math.max(1,end-Date.now()),'USB enumeration timed out');}catch{return false;}
  const candidate=previouslySelectedPort(ports,previous,excluded);
  if(candidate){try{await withDeadline(()=>open(candidate),Math.min(2000,Math.max(1,end-Date.now())),'USB open timed out');return true;}catch{try{await withDeadline(cleanup,1000,'USB cleanup timed out');}catch{return false;}}}
  await wait(500);
 }
 return false;
}

export function withDeadline(operation,ms,label){
 let timer;
 return Promise.race([Promise.resolve().then(operation),new Promise((_,reject)=>{timer=setTimeout(()=>reject(new Error(label)),ms);})]).finally(()=>clearTimeout(timer));
}

// esptool-js 0.5.4 cancels its reader but waits for locked === false without
// releasing the reader lock. cancel() alone does not release a Web Streams lock.
export async function closeFlashTransport(transport,timeoutMs=2000){
 const reader=transport.reader;
 if(reader){
  try{await withDeadline(()=>reader.cancel(),timeoutMs,'USB read cancellation timed out');}catch{/* Still release the owned lock below. */}
  reader.releaseLock();
  transport.reader=undefined;
 }
 const device=transport.device;
 if(device.readable?.locked||device.writable?.locked)throw Error('USB stream remains locked. Reconnect the controller and reload this page.');
 if(device.readable||device.writable)await withDeadline(()=>device.close(),timeoutMs,'USB close timed out. Reconnect the controller and reload this page.');
}
