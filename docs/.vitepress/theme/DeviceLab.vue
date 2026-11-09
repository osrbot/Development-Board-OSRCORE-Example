<script setup>
import {withBase} from 'vitepress';
import {fetchCourseFirmware,fetchFlashFiles} from './firmware-download.mjs';
import {useLanguage} from './language.mjs';
const {en,t}=useLanguage();
import {ref,computed,onMounted,onUnmounted,watch,nextTick} from 'vue';
import {X,Usb,Wifi,Download,RefreshCw,PlugZap} from 'lucide-vue-next';
import {exampleDirs,lessonHardwareEn,firmwareValues,firmwareParameters,lessonGuide} from './lesson-contract.mjs';
const panel=ref(null);let previousFocus;
const preset=ref(null),presetLoading=ref(false),presetError=ref(''),useCustom=ref(false),recovery=ref(false);let presetToken=0;
const say=(zh,enText)=>en.value?enText:zh;
const props=defineProps({open:Boolean,lesson:Number,values:Object});
const emit=defineEmits(['close','status']);
const transport=ref('usb'),connected=ref(false),busy=ref(false),error=ref(''),activity=ref(''),logs=ref([]),job=ref(null),buildInput=ref(''),available=ref(false),progress=ref(0),address=ref(''),token=ref(''),ssid=ref(''),password=ref(''),device=ref(null),command=ref('');
let port,reader,writer,readTask,pollTimer,buildTimer,networkAfter=0,disposed=false,reading=false;
const signature=computed(()=>JSON.stringify({lesson:props.lesson,parameters:firmwareValues(props.lesson,props.values||{})}));
const selectedBuild=computed(()=>useCustom.value?job.value:preset.value);
const canFlash=computed(()=>connected.value&&!busy.value&&selectedBuild.value?.state==='ready'&&selectedBuild.value.example===exampleDirs[props.lesson]&&(!useCustom.value||buildInput.value===signature.value)&&(transport.value==='usb'||useCustom.value));
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
async function closeUSB(){reading=false;try{await reader?.cancel();}catch{}try{await readTask;}catch{}try{writer?.releaseLock();}catch{}reader=null;writer=null;try{await port?.close();}catch{}}
async function disconnect(){clearTimeout(pollTimer);if(transport.value==='usb')await closeUSB();connected.value=false;device.value=null;status();}
async function readLoop(){const decoder=new TextDecoder();let pending='';try{while(reading){const r=await reader.read();if(r.done)break;pending+=decoder.decode(r.value,{stream:true});if(pending.length>8192)pending=pending.slice(-8192);const lines=pending.split(/\r?\n/);pending=lines.pop();for(const line of lines){const match=line.match(/WIFI ip=([\d.]+) token=([a-f0-9]{32})/);if(match){address.value=match[1]==='0.0.0.0'?'':match[1];token.value=match[2];log(address.value?t("Wi-Fi 已获取地址：")+address.value:t("Wi-Fi 尚未获取地址，请稍后再次读取。"));}else if(line.trim())log(line);}}}catch(e){if(reading)log(t("USB 断开：")+e.message);}finally{try{reader?.releaseLock();}catch{}reader=null;if(reading){connected.value=false;status();}}}
async function openMonitor(){await port.open({baudRate:460800});writer=port.writable.getWriter();reader=port.readable.getReader();reading=true;readTask=readLoop();connected.value=true;status();}
async function connectUSB(){error.value='';try{await closeUSB();port=await navigator.serial.requestPort({filters:[{usbVendorId:0x303a},{usbVendorId:0x10c4},{usbVendorId:0x1a86}]});busy.value=true;await openMonitor();recovery.value=false;log(t("USB 串口已连接。"));}catch(e){if(e.name!=='NotFoundError')error.value=e.message;await closeUSB();connected.value=false;status();}finally{busy.value=false;}}
async function sendLine(line){if(!writer)throw Error(t("请先连接 USB"));await writer.write(new TextEncoder().encode(line+'\n'));}
function hex(text){return Array.from(new TextEncoder().encode(text)).map(b=>b.toString(16).padStart(2,'0')).join('')||'-';}
async function configureWifi(){error.value='';try{if(new TextEncoder().encode(ssid.value).length>32||!ssid.value)throw Error(t("Wi-Fi 名称必须为 1–32 字节"));if(new TextEncoder().encode(password.value).length>63)throw Error(t("Wi-Fi 密码过长"));await sendLine(`wifi config ${hex(ssid.value)} ${hex(password.value)}`);password.value='';log(t("已发送配网信息；几秒后点击“读取配对信息”。"));}catch(e){error.value=e.message;}}
async function pair(){try{await sendLine('wifi status');}catch(e){error.value=e.message;}}
async function deviceApi(action,extra={}){return api('device',{action,address:address.value,token:token.value,...extra});}
async function connectWifi(){error.value='';busy.value=true;try{const info=await deviceApi('info');if(info.board!=='OSRCORE'||info.protocol!==1)throw Error(t("设备不是兼容的 OSRCORE 课程固件"));device.value=info;connected.value=true;networkAfter=0;log(`Wi-Fi 已连接：${info.project} / ${info.version}`);status();pollLogs();}catch(e){error.value=e.message;connected.value=false;status();}finally{busy.value=false;}}
async function pollLogs(){if(disposed||!connected.value||transport.value!=='wifi')return;try{if(!busy.value){const r=await deviceApi('logs',{after:networkAfter});networkAfter=r.next;r.lines.forEach(log);}}catch(e){log(t("设备暂不可达：")+e.message);connected.value=false;status();return;}pollTimer=setTimeout(pollLogs,1200);}
async function flash(){if(!canFlash.value)return;const build=selectedBuild.value;error.value='';recovery.value=false;busy.value=true;progress.value=0;activity.value=t("准备下载");try{
 if(transport.value==='wifi'){activity.value=t("通过 Wi-Fi 写入并校验固件");const r=await deviceApi('flash',{build:build.id});if(!r.ok)throw Error(t("开发板未确认写入完成"));progress.value=100;log(t("写入成功，开发板正在重启。重启后重新连接 Wi-Fi 验证程序版本。"));connected.value=false;status();}
 else {activity.value=say('正在获取并校验课程固件','Downloading and verifying course firmware');const fileArray=await fetchFlashFiles(build.files);await closeUSB();const {ESPLoader,Transport}=await import('esptool-js');const connection=new Transport(port);try{const loader=new ESPLoader({transport:connection,baudrate:460800,terminal:{clean(){},write:log,writeLine:log}});activity.value=t("进入下载模式");let chip;try{chip=await loader.main();}catch(e){recovery.value=true;throw e;}if(!chip.startsWith('ESP32-S3'))throw Error(t("仅支持 ESP32-S3，已停止写入")); activity.value=t("USB 写入固件");await loader.writeFlash({fileArray,flashSize:'keep',flashMode:'keep',flashFreq:'keep',eraseAll:false,compress:true,reportProgress:(file,written,total)=>progress.value=Math.round((file+written/total)/fileArray.length*100)});activity.value=t("校验开发板 Flash");for(const f of build.files){if(!f.md5||await loader.flashMd5sum(f.address,f.size)!==f.md5)throw Error(t("开发板 Flash 校验失败，请重新下载"));}await loader.after();progress.value=100;log(t("固件已写入并通过 Flash 校验；NVS 参数与配网信息保留。"));}finally{await connection.disconnect();}connected.value=false;status();activity.value=say('下载完成，正在重新连接','Download complete; reconnecting');try{await openMonitor();activity.value=say('下载完成，已连接日志','Download complete; console connected');}catch{await closeUSB();connected.value=false;status();activity.value=say('下载完成。点击连接 USB 查看日志。','Download complete. Connect USB to view logs.');}}
 }catch(e){error.value=e.message;await closeUSB();connected.value=false;status();}finally{busy.value=false;}}
async function sendCommand(text){error.value='';try{if(transport.value==='wifi')await deviceApi('command',{command:text});else await sendLine(text);log('> '+text);}catch(e){error.value=e.message;}}
async function switchTransport(next){if(busy.value)return;await disconnect();transport.value=next;useCustom.value=next==='wifi';error.value='';}
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
   <div class="device-steps"><span>{{t('1 连接开发板')}}</span><span>{{say('2 下载课程固件','2 Download lesson')}}</span><span>{{t('3 下载并看日志')}}</span></div>
   <nav class="device-tabs" :aria-label="t('连接方式')"><button :aria-pressed="transport==='usb'" :disabled="busy" @click="switchTransport('usb')"><Usb/>USB</button><button v-if="available" :aria-pressed="transport==='wifi'" :disabled="busy" @click="switchTransport('wifi')"><Wifi/>Wi-Fi</button></nav>
   <div v-if="transport==='usb'" class="device-connect">
    <p>{{say('插上 USB 数据线，连接后即可下载。本页面无需安装或部署服务。','Plug in a USB data cable, connect and download. No installation or local server is needed.')}}</p>
    <p v-if="!secure" class="device-notice"><a href="https://osrbot.github.io/osrcore-learn/">{{say('打开 HTTPS 正式站以使用 USB 下载','Open the HTTPS website to download over USB')}}</a></p>
    <p v-else-if="!supportedUSB" class="device-notice">{{t('当前浏览器未提供 Web Serial，请使用支持串口 API 的浏览器打开此页面。')}}</p>
    <button v-if="!connected" class="device-primary" :disabled="!supportedUSB||!secure||busy" @click="connectUSB">{{t('选择并连接 USB')}}</button><button v-else :disabled="busy" @click="disconnect">{{t('断开 USB')}}</button>
    <details v-if="connected&&!busy"><summary>{{t('为开发板配置 Wi-Fi')}}</summary><label>{{t('Wi-Fi 名称')}}<input v-model="ssid" autocomplete="off" /></label><label>{{t('Wi-Fi 密码')}}<input v-model="password" type="password" autocomplete="new-password" /></label><button @click="configureWifi">{{t('发送配网信息')}}</button><button @click="pair">{{t('读取配对信息')}}</button><p v-if="address">{{t('地址 ')}}{{address}}{{t(' · 配对信息已填入 Wi-Fi 页签。')}}</p><p>{{t('需先下载一次新版课程固件。密码仅发送到当前 USB 开发板。')}}</p></details>
   </div>
   <div v-else class="device-connect"><p>{{t('先通过 USB 给开发板配网。开发板和运行此本地服务的电脑需在同一局域网。')}}</p><label>{{t('开发板 IPv4 地址')}}<input v-model="address" placeholder="192.168.1.100" :disabled="connected||busy" /></label><label>{{t('USB 配对码')}}<input v-model="token" type="password" autocomplete="off" :disabled="connected||busy" /></label><button v-if="!connected" class="device-primary" :disabled="busy||!address||!token||!available" @click="connectWifi">{{t('连接 Wi-Fi 开发板')}}</button><button v-else :disabled="busy" @click="disconnect">{{t('断开 Wi-Fi')}}</button><p v-if="device">{{t('运行程序：')}}{{device.project}} · {{device.version}}</p></div>
   <section class="device-build"><h3>{{t('本课固件 · ')}}{{exampleDirs[lesson]||t('认识开发板')}}</h3><p>{{en?lessonHardwareEn[lesson]:lessonGuide[lesson]?.hardware}}</p>
    <p v-if="!lesson">{{say('请先选择 01–13 中的一节课程。','Choose a lesson from 01 to 13 first.')}}</p>
    <template v-else>
     <p v-if="!useCustom">{{say('下载本课默认固件；网页滑条仅影响模拟，不改变此固件的参数。','Installs this lesson’s default firmware. Simulation sliders do not change these firmware parameters.')}}</p>
     <p v-if="presetLoading&&!useCustom" role="status">{{say('正在获取课程固件…','Loading course firmware…')}}</p>
     <p v-if="presetError&&!useCustom" role="alert">{{presetError}} <button :disabled="busy" @click="loadPreset">{{say('重试','Retry')}}</button></p>
     <button class="device-primary" :disabled="!canFlash" @click="flash"><Download/>{{busy?say('正在下载…','Downloading…'):t('下载到开发板')}}</button>
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
   <p v-if="error" class="device-error" role="alert">{{t(error)}}</p>
   <section class="device-console"><header><h3>{{t('真实设备日志')}}</h3><button @click="logs=[]">{{t('清空')}}</button></header><pre role="log" :aria-label="t('真实设备日志')">{{logs.join('\n')||t('连接后显示开发板日志；这里不显示网页模拟数据。')}}</pre><div v-if="connected&&!busy&&lesson===7"><button @click="sendCommand('save')">{{t('保存 Kp')}}</button><button @click="sendCommand('reboot')">{{t('重启并读回')}}</button></div><form v-if="connected&&!busy&&transport==='usb'" @submit.prevent="sendCommand(command);command=''"><input v-model="command" :placeholder="t('串口命令，如 status')" :aria-label="t('串口命令')"/><button :disabled="!command.trim()">{{t('发送')}}</button></form></section>
  </div>
 </section>
</template>
<style scoped>
.device-backdrop{position:fixed;inset:0;z-index:100;background:#152b3c44;display:flex;justify-content:flex-end;backdrop-filter:blur(3px)}.device-panel{background:#fbfdff;width:min(620px,100%);height:100%;overflow:auto;padding:28px;box-shadow:-12px 0 50px #16324d20}.device-panel>header,.device-console header{display:flex;align-items:center;justify-content:space-between}.device-panel h2{font-size:23px;font-weight:650}.device-panel h3{font-size:15px;font-weight:650}.device-panel p{font-size:12px;line-height:1.8;color:#52697a;margin:10px 0}.device-panel button{display:inline-flex;align-items:center;justify-content:center;gap:7px;border:1px solid #cedde7;border-radius:8px;padding:8px 12px;background:white;margin:4px 6px 4px 0;font-size:12px;min-height:36px}.device-panel button:disabled{opacity:.45;cursor:not-allowed}.device-panel .device-primary{background:#007cda;color:white;border-color:#007cda}.device-steps{display:flex;gap:16px;color:#668194;font-size:11px;margin:20px 0}.device-tabs{display:flex;border-bottom:1px solid #dce6ed;padding-bottom:8px}.device-tabs button[aria-pressed=true]{background:#e9f5ff;border-color:#008ddd;color:#0073bf}.device-panel label{display:block;margin:10px 0;font-size:12px;color:#52697a}.device-panel input{display:block;width:100%;padding:10px;border:1px solid #ccdae5;border-radius:7px;background:white;margin-top:5px;color:#1e3447}.device-build,.device-console{border-top:1px solid #dce6ed;margin-top:20px;padding-top:18px}.device-panel pre{max-height:200px;overflow:auto;white-space:pre-wrap;word-break:break-word;background:#f0f6fa;border:1px solid #e0e9ef;border-radius:8px;padding:12px;font:11px/1.7 monospace;color:#28495d}.device-panel summary{cursor:pointer;font-size:12px;padding:10px 0;color:#0078c8}.device-panel .device-error{color:#aa3434;background:#fff0ed;border-radius:7px;padding:12px}.device-panel .device-notice{background:#fff8e8;padding:10px;border-radius:7px}.device-notice a{color:#0078c8;text-decoration:underline}.device-console form{display:flex;gap:8px}.device-console form input{min-width:0}.device-panel code{font-size:11px}
</style>
