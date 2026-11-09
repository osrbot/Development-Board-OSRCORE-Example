<script setup>
import {resetS3ToApplication,reconnectAuthorizedUSB,closeFlashTransport,withDeadline} from './usb-session.mjs';
import {withBase} from 'vitepress';
import {parseDeviceInfo,deviceProblem,troubleshooting,physicalLessons} from './device-experience.mjs';
import {fetchCourseFirmware,fetchFlashFiles} from './firmware-download.mjs';
import {useLanguage} from './language.mjs';
const {en,t}=useLanguage();
import {ref,computed,onMounted,onUnmounted,watch,nextTick} from 'vue';
import {X,Usb,Wifi,Download,RefreshCw} from 'lucide-vue-next';
import {exampleDirs,lessonHardwareEn,firmwareValues,firmwareParameters,lessonGuide} from './lesson-contract.mjs';
const panel=ref(null);let previousFocus;
const preset=ref(null),presetLoading=ref(false),presetError=ref(''),useCustom=ref(false),recovery=ref(false);let presetToken=0;
const say=(zh,enText)=>en.value?enText:zh;
const props=defineProps({open:Boolean,lesson:Number,values:Object});
const emit=defineEmits(['close','status']);
const transport=ref('usb'),connected=ref(false),busy=ref(false),error=ref(''),activity=ref(''),logs=ref([]),job=ref(null),buildInput=ref(''),available=ref(false),progress=ref(0),address=ref(''),token=ref(''),ssid=ref(''),password=ref(''),device=ref(null),command=ref('');
const lastFlash=ref(null),consoleSection=ref(null),nextSection=ref(null);
let infoTimers=[];
const confirmedLesson=computed(()=>device.value?exampleDirs.indexOf(device.value.project):-1);
const guideLesson=computed(()=>device.value?(confirmedLesson.value>0?confirmedLesson.value:null):lastFlash.value?.lesson);
const physicalGuide=computed(()=>physicalLessons[guideLesson.value]?.[en.value?'en':'zh']);
const problem=computed(()=>troubleshooting[deviceProblem(error.value)]?.[en.value?'en':'zh']);
function stopInfoQuery(){infoTimers.forEach(clearTimeout);infoTimers=[];}
function queryInfo(){
 stopInfoQuery();if(transport.value!=='usb'||!connected.value)return;
 const epoch=monitorEpoch;
 for(const delay of [0,2000,6000])infoTimers.push(setTimeout(async()=>{
  if(epoch!==monitorEpoch||!connected.value||disposed)return;
  try{await withDeadline(()=>sendLine('osrcore info'),1500,'Device query timed out');}catch(e){log(e.message);}
 },delay));
}
function showConsole(){consoleSection.value?.scrollIntoView({behavior:'smooth',block:'start'});}
let monitorEpoch=0;
let port,reader,writer,readTask,pollTimer,buildTimer,networkAfter=0,disposed=false,reading=false;
const signature=computed(()=>JSON.stringify({lesson:props.lesson,parameters:firmwareValues(props.lesson,props.values||{})}));
const selectedBuild=computed(()=>useCustom.value?job.value:preset.value);
const readyBuild=computed(()=>!busy.value&&selectedBuild.value?.state==='ready'&&selectedBuild.value.example===exampleDirs[props.lesson]&&(!useCustom.value||buildInput.value===signature.value)&&(transport.value==='usb'||useCustom.value));
const canFlash=computed(()=>connected.value&&readyBuild.value);
const canDownload=computed(()=>readyBuild.value&&(transport.value==='usb'?supportedUSB.value&&secure.value:connected.value));
async function downloadLesson(){if(!canDownload.value)return;if(!connected.value)await connectUSB();if(connected.value)await flash();}
async function loadPreset(){const token=++presetToken;preset.value=null;presetError.value='';if(!props.lesson){presetLoading.value=false;return;}presetLoading.value=true;try{const result=await fetchCourseFirmware(withBase('/firmware/'),exampleDirs[props.lesson]);if(token===presetToken)preset.value=result;}catch(e){if(token===presetToken)presetError.value=say('课程固件暂时不可用，请重试。','Course firmware is unavailable. Please retry.');}finally{if(token===presetToken)presetLoading.value=false;}}
const supportedUSB=ref(false),secure=ref(false);
const requestingBuild=ref(false);
const building=computed(()=>requestingBuild.value||['queued','building'].includes(job.value?.state));
const parameterText=computed(()=>Object.keys(firmwareParameters[props.lesson]||{}).length?t("当前可下载参数会在编译时写入程序。标为“模拟输入”的参数不会发送给真板。"):t("本课下载真实传感器／控制例程；网页输入仅用于模拟。"));
function log(line){logs.value.push(String(line).replace(/\x1b\[[0-9;]*m/g,''));if(logs.value.length>160)logs.value.splice(0,logs.value.length-160);}
async function api(route,body){const r=await fetch('/api/lab/'+route,{method:body?'POST':'GET',headers:body?{'Content-Type':'application/json','X-OSRCORE-Lab':'1'}:{},body:body?JSON.stringify(body):undefined});const d=await r.json();if(!r.ok)throw Error(d.error||`HTTP ${r.status}`);return d;}
async function health(){if(location.hostname.endsWith('.github.io')){available.value=false;return;}try{const r=await api('health');available.value=r.local===true;}catch{available.value=false;}}
async function compile(){if(building.value)return;requestingBuild.value=true;error.value='';job.value=null;buildInput.value=signature.value;try{job.value=await api('builds',JSON.parse(buildInput.value));await pollBuild();}catch(e){error.value=e.message;}finally{requestingBuild.value=false;}}
async function pollBuild(){if(disposed||!job.value)return;try{job.value=await api('builds/'+job.value.id);if(['queued','building'].includes(job.value.state))buildTimer=setTimeout(pollBuild,1200);else if(job.value.state==='error')error.value=job.value.error;}catch(e){error.value=e.message;}}
function status(){emit('status',connected.value?(transport.value==='usb'?t("USB 已连接"):t("Wi-Fi 已连接")):'');}
async function closeUSB(){stopInfoQuery();device.value=null;monitorEpoch++;reading=false;const oldReader=reader,oldWriter=writer,oldTask=readTask,oldPort=port;reader=null;writer=null;try{await withDeadline(()=>oldReader?.cancel(),1000,'USB cancel timed out');}catch{}try{oldReader?.releaseLock();}catch{}try{await withDeadline(()=>oldTask,1000,'USB reader timed out');}catch{}try{oldWriter?.releaseLock();}catch{}try{await withDeadline(()=>oldPort?.close(),2000,'USB close timed out');}catch{}}
async function disconnect(){clearTimeout(pollTimer);if(transport.value==='usb')await closeUSB();connected.value=false;device.value=null;status();}
async function readLoop(sourceReader,epoch){const decoder=new TextDecoder();let pending='';try{while(reading&&epoch===monitorEpoch){const r=await sourceReader.read();if(r.done)break;pending+=decoder.decode(r.value,{stream:true});if(pending.length>8192)pending=pending.slice(-8192);const lines=pending.split(/\r?\n/);pending=lines.pop();for(const line of lines){const match=line.match(/WIFI ip=([\d.]+) token=([a-f0-9]{32})/);if(match){address.value=match[1]==='0.0.0.0'?'':match[1];token.value=match[2];log(address.value?t("Wi-Fi 已获取地址：")+address.value:t("Wi-Fi 尚未获取地址，请稍后再次读取。"));}else if(line.trim()){const info=parseDeviceInfo(line);if(info){device.value=info;stopInfoQuery();}log(line);}}}}catch(e){if(reading&&epoch===monitorEpoch)log(t("USB 断开：")+e.message);}finally{try{sourceReader.releaseLock();}catch{}if(epoch===monitorEpoch)reader=null;if(reading&&epoch===monitorEpoch){connected.value=false;device.value=null;stopInfoQuery();status();}}}
async function openMonitor(){const epoch=monitorEpoch,candidate=port;await candidate.open({baudRate:460800});if(epoch!==monitorEpoch||disposed){try{await candidate.close();}catch{}throw Error('USB connection cancelled');}writer=candidate.writable.getWriter();reader=candidate.readable.getReader();reading=true;readTask=readLoop(reader,epoch);connected.value=true;status();queryInfo();}
async function connectUSB(){error.value='';busy.value=true;try{await closeUSB();const previous=(await navigator.serial.getPorts()).includes(port)?port:null;if(!previous)lastFlash.value=null;port=previous||await navigator.serial.requestPort({filters:[{usbVendorId:0x303a},{usbVendorId:0x10c4},{usbVendorId:0x1a86}]});await openMonitor();recovery.value=false;log(t("USB 串口已连接。"));}catch(e){if(e.name!=='NotFoundError')error.value=e.message;await closeUSB();connected.value=false;status();}finally{busy.value=false;}}
async function sendLine(line){if(!writer)throw Error(t("请先连接 USB"));const target=writer;await withDeadline(()=>target.write(new TextEncoder().encode(line+'\n')),2000,'USB command timed out');}
function hex(text){return Array.from(new TextEncoder().encode(text)).map(b=>b.toString(16).padStart(2,'0')).join('')||'-';}
async function configureWifi(){error.value='';try{if(new TextEncoder().encode(ssid.value).length>32||!ssid.value)throw Error(t("Wi-Fi 名称必须为 1–32 字节"));if(new TextEncoder().encode(password.value).length>63)throw Error(t("Wi-Fi 密码过长"));await sendLine(`wifi config ${hex(ssid.value)} ${hex(password.value)}`);password.value='';log(t("已发送配网信息；几秒后点击“读取配对信息”。"));}catch(e){error.value=e.message;}}
async function pair(){try{await sendLine('wifi status');}catch(e){error.value=e.message;}}
async function deviceApi(action,extra={}){return api('device',{action,address:address.value,token:token.value,...extra});}
async function connectWifi(){error.value='';busy.value=true;try{const info=await deviceApi('info');if(info.board!=='OSRCORE'||info.protocol!==1)throw Error(t("设备不是兼容的 OSRCORE 课程固件"));device.value=info;connected.value=true;networkAfter=0;log(say('Wi-Fi 已连接：','Wi-Fi connected: ')+`${info.project} / ${info.version}`);status();pollLogs();}catch(e){error.value=e.message;connected.value=false;status();}finally{busy.value=false;}}
async function pollLogs(){if(disposed||!connected.value||transport.value!=='wifi')return;try{if(!busy.value){const r=await deviceApi('logs',{after:networkAfter});networkAfter=r.next;r.lines.forEach(log);}}catch(e){log(t("设备暂不可达：")+e.message);connected.value=false;status();return;}pollTimer=setTimeout(pollLogs,1200);}
async function flash(){if(!canFlash.value)return;const build=selectedBuild.value;const flashingLesson=props.lesson;lastFlash.value=null;device.value=null;stopInfoQuery();error.value='';recovery.value=false;busy.value=true;progress.value=0;activity.value=t("准备下载");try{
 if(transport.value==='wifi'){activity.value=t("通过 Wi-Fi 写入并校验固件");const r=await deviceApi('flash',{build:build.id});if(!r.ok)throw Error(t("开发板未确认写入完成"));lastFlash.value={lesson:flashingLesson,example:build.example};progress.value=100;log(t("写入成功，开发板正在重启。重启后重新连接 Wi-Fi 验证程序版本。"));connected.value=false;status();}
 else {const otherPorts=(await navigator.serial.getPorts()).filter(p=>p!==port);activity.value=say('正在获取并校验课程固件','Downloading and verifying course firmware');const fileArray=await fetchFlashFiles(build.files);await closeUSB();const {ESPLoader,Transport}=await import('esptool-js');const connection=new Transport(port);try{const loader=new ESPLoader({transport:connection,baudrate:460800,terminal:{clean(){},write:log,writeLine:log}});activity.value=t("进入下载模式");let chip;try{chip=await loader.main();}catch(e){recovery.value=true;throw e;}if(!chip.startsWith('ESP32-S3'))throw Error(t("仅支持 ESP32-S3，已停止写入")); activity.value=t("USB 写入固件");await loader.writeFlash({fileArray,flashSize:'keep',flashMode:'keep',flashFreq:'keep',eraseAll:false,compress:true,reportProgress:(file,written,total)=>progress.value=Math.round((file+written/total)/fileArray.length*90)});progress.value=90;activity.value=t("校验开发板 Flash");log(activity.value);for(const f of build.files){if(!f.md5||await withDeadline(()=>loader.flashMd5sum(f.address,f.size),15000,say('Flash 校验超时，尚未确认下载成功。','Flash verification timed out; download is not confirmed.'))!==f.md5)throw Error(t("开发板 Flash 校验失败，请重新下载"));}lastFlash.value={lesson:flashingLesson,example:build.example};progress.value=95;activity.value=say('正在重启控制器','Restarting controller');try{await resetS3ToApplication(loader,connection);}catch(e){log(say('固件校验成功，但复位应答中断；将尝试自动重连。','Firmware verified; reset acknowledgement interrupted. Attempting reconnection.'));}progress.value=98;log(t("固件已写入并通过 Flash 校验；NVS 参数与配网信息保留。"));}finally{activity.value=say('正在释放 USB 烧录连接','Releasing USB flasher connection');log(activity.value);await closeFlashTransport(connection);}connected.value=false;status();activity.value=say('下载完成，正在重新连接','Download complete; reconnecting');const previous=port;const resumed=await reconnectAuthorizedUSB({serial:navigator.serial,previous,excluded:otherPorts,open:async candidate=>{port=candidate;await openMonitor();},cleanup:closeUSB,cancelled:()=>disposed});progress.value=100;activity.value=resumed?say('下载完成，已自动连接日志','Download complete; console reconnected'):say('下载完成，设备尚未重新出现。检查 USB 连接后重连。','Download complete; device has not returned. Check USB and reconnect.');}

 }catch(e){error.value=e.message;log(error.value);activity.value=say('下载流程已停止，请查看提示。','Download process stopped. See the message below.');await closeUSB();connected.value=false;status();}finally{busy.value=false;if(!error.value&&lastFlash.value&&props.open){await nextTick();nextSection.value?.scrollIntoView({behavior:'smooth',block:'start'});}}}
async function restartController(){
 if(!connected.value||busy.value||!device.value)return;
 error.value='';busy.value=true;
 try{
  if(transport.value==='wifi'){await deviceApi('command',{command:'reboot'});await disconnect();log(say('已请求重启，请重新连接 Wi-Fi。','Restart requested. Reconnect over Wi-Fi.'));return;}
  stopInfoQuery();const previous=port;const others=(await navigator.serial.getPorts()).filter(p=>p!==previous);
  await sendLine('osrcore reboot');device.value=null;
  await new Promise(resolve=>setTimeout(resolve,500));
  await closeUSB();connected.value=false;status();
  const resumed=await reconnectAuthorizedUSB({serial:navigator.serial,previous,excluded:others,open:async candidate=>{port=candidate;await openMonitor();},cleanup:closeUSB,cancelled:()=>disposed});
  log(resumed?say('重启后已连接日志，正在查询设备信息。','Console reconnected after restart; querying device information.'):say('设备未重新出现，请检查 USB 后重连。','Device has not returned. Check USB and reconnect.'));
 }catch(e){error.value=e.message;}finally{busy.value=false;}
}
async function sendCommand(text){error.value='';try{if(transport.value==='wifi')await deviceApi('command',{command:text});else await sendLine(text);log('> '+text);}catch(e){error.value=e.message;}}
async function switchTransport(next){if(busy.value)return;await disconnect();transport.value=next;lastFlash.value=null;useCustom.value=next==='wifi';error.value='';}
function keyboard(event){if(!props.open)return;if(event.key==='Escape'&&!busy.value){emit('close');return;}if(event.key==='Tab'){const items=Array.from(panel.value?.querySelectorAll('button:not(:disabled),input:not(:disabled),a[href],summary')||[]).filter(el=>el.getClientRects().length);if(!items.length)return;const first=items[0],last=items.at(-1);if(event.shiftKey&&document.activeElement===first){event.preventDefault();last.focus();}else if(!event.shiftKey&&document.activeElement===last){event.preventDefault();first.focus();}}}
watch(()=>props.open,async open=>{if(open){if(!preset.value&&!presetLoading.value)loadPreset();previousFocus=document.activeElement;await nextTick();panel.value?.querySelector('button')?.focus();}else previousFocus?.focus();});
watch(()=>props.lesson,()=>{error.value='';if(props.open)loadPreset();else{presetToken++;preset.value=null;presetLoading.value=false;}});
onMounted(()=>{document.addEventListener('keydown',keyboard);secure.value=window.isSecureContext;supportedUSB.value='serial' in navigator;health();});
onUnmounted(()=>{document.removeEventListener('keydown',keyboard);disposed=true;clearTimeout(buildTimer);clearTimeout(pollTimer);closeUSB();});
</script>
<template>
 <section v-show="open" class="device-backdrop" @click.self="!busy&&emit('close')">
  <div ref="panel" class="device-panel" role="dialog" aria-modal="true" aria-labelledby="device-title">
   <header><div><span class="lesson-eyebrow">{{t('真实开发板')}}</span><h2 id="device-title">{{t('从网页到 OSRCORE')}}</h2></div><button :disabled="busy" @click="emit('close')" :aria-label="t('关闭设备面板')"><X/></button></header>
   <div class="device-steps"><span>{{t('1 连接开发板')}}</span><span>{{say('2 下载课程固件','2 Download lesson')}}</span><span>{{say('3 操作与观察','3 Try and observe')}}</span></div>
   <section class="device-state" aria-live="polite"><div><i :class="{online:connected}"/>{{connected?say('已连接','Connected'):say('未连接','Disconnected')}} · {{transport==='usb'?'USB':'Wi-Fi'}}</div><p v-if="device">{{say('设备报告运行课程','Device reports lesson')}} · {{device.project}}<br/>{{say('版本','Version')}} · {{device.version}}</p><p v-else-if="connected">{{say('串口已连接，尚未收到课程固件的身份应答。旧版固件可正常下载，更新后支持设备识别。','Port connected; no course identity reply received yet. Older firmware can still be flashed; update it to enable identification.')}}</p><p v-if="lastFlash">{{say('本次已写入并校验','Written and verified this session')}} · {{lastFlash.example}}</p></section>
   <nav class="device-tabs" :aria-label="t('连接方式')"><button :aria-pressed="transport==='usb'" :disabled="busy" @click="switchTransport('usb')"><Usb/>USB</button><button v-if="available" :aria-pressed="transport==='wifi'" :disabled="busy" @click="switchTransport('wifi')"><Wifi/>Wi-Fi</button></nav>
   <div v-if="transport==='usb'" class="device-connect">
    <p>{{say('插上 USB 数据线，连接后即可下载。本页面无需安装或部署服务。','Plug in a USB data cable, connect and download. No installation or local server is needed.')}}</p>
    <p v-if="!secure" class="device-notice"><a href="https://osrbot.github.io/osrcore-learn/">{{say('打开 HTTPS 正式站以使用 USB 下载','Open the HTTPS website to download over USB')}}</a></p>
    <p v-else-if="!supportedUSB" class="device-notice">{{t('当前浏览器未提供 Web Serial，请使用支持串口 API 的浏览器打开此页面。')}}</p>
    <button v-if="!connected" :disabled="!supportedUSB||!secure||busy" @click="connectUSB">{{t('选择并连接 USB')}}</button><button v-else :disabled="busy" @click="disconnect">{{t('断开 USB')}}</button>
    <details v-if="connected&&!busy"><summary>{{t('为开发板配置 Wi-Fi')}}</summary><label>{{t('Wi-Fi 名称')}}<input v-model="ssid" autocomplete="off" /></label><label>{{t('Wi-Fi 密码')}}<input v-model="password" type="password" autocomplete="new-password" /></label><button @click="configureWifi">{{t('发送配网信息')}}</button><button @click="pair">{{t('读取配对信息')}}</button><p v-if="address">{{t('地址 ')}}{{address}}{{t(' · 配对信息已填入 Wi-Fi 页签。')}}</p><p>{{t('需先下载一次新版课程固件。密码仅发送到当前 USB 开发板。')}}</p></details>
   </div>
   <div v-else class="device-connect"><p>{{t('先通过 USB 给开发板配网。开发板和运行此本地服务的电脑需在同一局域网。')}}</p><label>{{t('开发板 IPv4 地址')}}<input v-model="address" placeholder="192.168.1.100" :disabled="connected||busy" /></label><label>{{t('USB 配对码')}}<input v-model="token" type="password" autocomplete="off" :disabled="connected||busy" /></label><button v-if="!connected" class="device-primary" :disabled="busy||!address||!token||!available" @click="connectWifi">{{t('连接 Wi-Fi 开发板')}}</button><button v-else :disabled="busy" @click="disconnect">{{t('断开 Wi-Fi')}}</button><p v-if="device">{{t('运行程序：')}}{{device.project}} · {{device.version}}</p></div>
   <section class="device-build"><h3>{{t('本课固件 · ')}}{{exampleDirs[lesson]||t('认识开发板')}}</h3><p>{{en?lessonHardwareEn[lesson]:lessonGuide[lesson]?.hardware}}</p>
    <p v-if="!lesson">{{say('请先选择 01–13 中的一节课程。','Choose a lesson from 01 to 13 first.')}}</p>
    <template v-else>
     <p v-if="!useCustom">{{say('下载本课默认固件；网页滑条仅影响模拟，不改变此固件的参数。','Installs this lesson’s default firmware. Simulation sliders do not change these firmware parameters.')}}</p>
     <p v-if="presetLoading&&!useCustom" role="status">{{say('正在获取课程固件…','Loading course firmware…')}}</p>
     <p v-if="presetError&&!useCustom" role="alert">{{presetError}} <button :disabled="busy" @click="loadPreset">{{say('重试','Retry')}}</button></p>
     <button class="device-primary" :disabled="!canDownload" @click="downloadLesson"><Download/>{{busy?say('请稍候…','Please wait…'):connected?t('下载到开发板'):say('连接并下载本课','Connect and download lesson')}}</button>
     <p v-if="preset&&!useCustom">{{say('固件版本','Firmware revision')}} · {{preset.revision.slice(0,8)}}</p>
     <details v-if="available"><summary>{{say('高级：编译自定义参数','Advanced: compile custom parameters')}}</summary>
      <label><input v-model="useCustom" type="checkbox" :disabled="busy||transport==='wifi'"/>{{say('使用本地编译固件','Use locally compiled firmware')}}</label>
      <template v-if="useCustom"><p>{{parameterText}}</p><button :disabled="building||busy" @click="compile"><RefreshCw/>{{building?t('正在编译…'):t('编译当前参数')}}</button>
      <p v-if="job" role="status">{{job.state}} · {{job.example}}</p><p v-if="job&&buildInput!==signature">{{t('课程或参数已改变，请重新编译后下载。')}}</p>
      <details v-if="job?.log"><summary>{{t('构建日志')}}</summary><pre>{{job.log}}</pre></details></template>
     </details>
    </template>
    <details v-if="transport==='usb'" :open="recovery"><summary>{{say('无法连接？首次安装 / BOOT 恢复','Cannot connect? First installation / BOOT recovery')}}</summary><p>{{say('通常无需按键。若无法连接：按住 B（BOOT），按下并松开 R（RESET），再松开 B。重新选择 USB 设备后下载；写入完成后如未运行，再按一次 R。','Normally no buttons are needed. If connection fails: hold B (BOOT), press and release R (RESET), then release B. Select the USB device again and download. If the program does not start after flashing, press R once.')}}</p><p>{{say('仅下载 OSRCORE 对应固件。写入会替换当前程序，但不擦除本项目分区布局中的 NVS 参数。','Install OSRCORE firmware only. Downloading replaces the current program while preserving NVS for this project’s partition layout.')}}</p></details>
    <p v-if="activity">{{t(activity)}} <span v-if="progress">{{progress}}%</span></p>
   </section>
   <section v-if="error" class="device-error" role="alert"><h3>{{problem[0]}}</h3><p>{{problem[1]}}</p><details><summary>{{say('技术详情','Technical details')}}</summary><pre>{{error}}</pre></details></section>
   <section v-if="physicalGuide" ref="nextSection" class="device-next"><span class="lesson-eyebrow">{{say('实板体验','ON YOUR CONTROLLER')}} · {{String(guideLesson).padStart(2,'0')}}</span><h3>{{say('接下来试一试','Try it now')}}</h3><p>{{physicalGuide[0]}}</p><p><strong>{{say('预期表现：','What to expect: ')}}</strong>{{physicalGuide[1]}}</p><p v-if="!device" class="device-unconfirmed">{{say('以下是这节课的预期表现，尚未确认设备程序已启动。','These are the expected results for this lesson; application startup has not been confirmed.')}}</p><p v-else>{{say('设备已响应身份查询；传感器和外设是否正常，请结合日志与实际表现判断。','The device has answered the identity query. Check logs and actual behavior to verify sensors and peripherals.')}}</p><button @click="showConsole">{{say('查看设备日志','View device console')}}</button></section>
   <section ref="consoleSection" class="device-console"><header><h3>{{t('真实设备日志')}}</h3><button @click="logs=[]">{{t('清空')}}</button></header><div v-if="connected&&!busy"><button v-if="transport==='usb'" @click="queryInfo">{{say('读取设备信息','Read device info')}}</button><button :disabled="!device" @click="restartController"><RefreshCw/>{{say('重启控制器','Restart controller')}}</button></div><pre role="log" :aria-label="t('真实设备日志')">{{logs.join('\n')||t('连接后显示开发板日志；这里不显示网页模拟数据。')}}</pre><div v-if="connected&&!busy&&confirmedLesson===7"><button @click="sendCommand('save')">{{t('保存 Kp')}}</button></div><form v-if="connected&&!busy&&transport==='usb'" @submit.prevent="sendCommand(command);command=''"><input v-model="command" :placeholder="t('串口命令，如 status')" :aria-label="t('串口命令')"/><button :disabled="!command.trim()">{{t('发送')}}</button></form></section>
   <details class="device-help"><summary>{{say('连接与下载帮助','Connection and download help')}}</summary><section v-for="(item,key) in troubleshooting" :key="key"><h3>{{item[en?'en':'zh'][0]}}</h3><p>{{item[en?'en':'zh'][1]}}</p></section></details>
  </div>
 </section>
</template>
<style scoped>
.device-backdrop{position:fixed;inset:0;z-index:100;background:#152b3c44;display:flex;justify-content:flex-end;backdrop-filter:blur(3px)}.device-panel{background:#fbfdff;width:min(620px,100%);height:100%;overflow:auto;padding:28px;box-shadow:-12px 0 50px #16324d20}.device-panel>header,.device-console header{display:flex;align-items:center;justify-content:space-between}.device-panel h2{font-size:23px;font-weight:650}.device-panel h3{font-size:15px;font-weight:650}.device-panel p{font-size:12px;line-height:1.8;color:#52697a;margin:10px 0}.device-panel button{display:inline-flex;align-items:center;justify-content:center;gap:7px;border:1px solid #cedde7;border-radius:8px;padding:8px 12px;background:white;margin:4px 6px 4px 0;font-size:12px;min-height:36px}.device-panel button:disabled{opacity:.45;cursor:not-allowed}.device-panel .device-primary{background:#007cda;color:white;border-color:#007cda}.device-steps{display:flex;gap:16px;color:#668194;font-size:11px;margin:20px 0}.device-tabs{display:flex;border-bottom:1px solid #dce6ed;padding-bottom:8px}.device-tabs button[aria-pressed=true]{background:#e9f5ff;border-color:#008ddd;color:#0073bf}.device-panel label{display:block;margin:10px 0;font-size:12px;color:#52697a}.device-panel input{display:block;width:100%;padding:10px;border:1px solid #ccdae5;border-radius:7px;background:white;margin-top:5px;color:#1e3447}.device-build,.device-console{border-top:1px solid #dce6ed;margin-top:20px;padding-top:18px}.device-panel pre{max-height:200px;overflow:auto;white-space:pre-wrap;word-break:break-word;background:#f0f6fa;border:1px solid #e0e9ef;border-radius:8px;padding:12px;font:11px/1.7 monospace;color:#28495d}.device-panel summary{cursor:pointer;font-size:12px;padding:10px 0;color:#0078c8}.device-panel .device-error{color:#aa3434;background:#fff0ed;border-radius:7px;padding:12px}.device-panel .device-notice{background:#fff8e8;padding:10px;border-radius:7px}.device-notice a{color:#0078c8;text-decoration:underline}.device-console form{display:flex;gap:8px}.device-console form input{min-width:0}.device-panel code{font-size:11px}
.device-state{border:1px solid #dce6ed;border-radius:10px;padding:12px 16px;background:white;margin-bottom:12px;font-size:13px}.device-state i{display:inline-block;width:8px;height:8px;background:#93a4b3;border-radius:50%;margin-right:6px}.device-state i.online{background:#0076df}.device-state p{overflow-wrap:anywhere;margin:6px 0 0}.device-next{padding:18px;border:1px solid #b9d9f4;border-radius:12px;background:#eef7ff;margin-top:20px}.device-next h3{margin-top:5px;font-size:18px}.device-panel .device-unconfirmed{color:#7b601d}.device-console,.device-next{scroll-margin-top:16px}.device-help{margin-top:18px;border-top:1px solid #dce6ed}.device-help section{margin:12px 0}.device-error h3{color:#962c2c}.device-panel .device-error p{color:#813b37}.device-panel svg{width:18px;height:18px;flex-shrink:0}
</style>
