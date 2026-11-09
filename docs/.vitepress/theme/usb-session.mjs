const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));

// ESP32-S3 force-download workaround from Espressif's esp32s3.py hard_reset.
// https://github.com/espressif/esptool/blob/master/esptool/targets/esp32s3.py
export async function resetS3ToApplication(loader,transport,wait=pause){
 await loader.writeReg(0x6000812c,0,1);
 await transport.setDTR(false);
 await transport.setRTS(true);
 await wait(200);
 await transport.setRTS(false);
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
export async function reconnectAuthorizedUSB({serial,previous,open,cleanup,wait=pause,cancelled=()=>false,attempts=30,excluded=[]}){
 for(let i=0;i<attempts;i++){
  if(cancelled())return false;
  const candidate=previouslySelectedPort(await serial.getPorts(),previous,excluded);
  if(candidate){try{await open(candidate);return true;}catch{await cleanup();}}
  await wait(500);
 }
 return false;
}
