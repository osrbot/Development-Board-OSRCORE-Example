<script setup>
import {useLanguage} from './language.mjs';
const {en,t}=useLanguage();
import {computed,ref,watch} from 'vue';
import ConnectorDiagram from './ConnectorDiagram.vue';
import {hardwareInEnglish} from './hardware-en.mjs';
import {hardware} from './hardware-map.mjs';
const props=defineProps({id:String,preferredGpio:Number,running:Boolean,output:Object,flow:Boolean});const emit=defineEmits(['pin']);
const part=computed(()=>{const h=hardware.find(h=>h.id===props.id)||hardware[6];return en.value?hardwareInEnglish(h):h;});
const pin=ref('');watch(()=>[props.id,props.preferredGpio],()=>{pin.value=(props.preferredGpio!=null?part.value.pins.find(p=>p.gpio===props.preferredGpio)?.n:null)||part.value.pins.find(p=>p.gpio!=null)?.n||part.value.pins[0]?.n||'';emit('pin',pin.value)},{immediate:true});
watch(pin,p=>emit('pin',p),{immediate:true});
const chosen=computed(()=>part.value.pins.find(p=>p.n===pin.value));
const pins=computed(()=>part.value.reverse?[...part.value.pins].reverse():part.value.pins);
const isSocket=computed(()=>['socket','xt30'].includes(part.value.kind));
const typeLabel=computed(()=>part.value.kind==='xt30'?t("电源 / 信号"):part.value.kind==='socket'?part.value.pins.length+' PIN':part.value.kind==='header'?'3 × 8 PIN':t("板载元件"));
const signal=computed(()=>props.id==='RGB'?'ESP32-S3 · GPIO45 → DIN → WS2812':props.id==='IMU'?'ESP32-S3 · GPIO10 / 11 ↔ I²C ↔ QMI8658A':props.id==='EX1'?(chosen.value?.gpio!=null?`ESP32-S3 · GPIO${chosen.value.gpio} → EX1 · PIN ${chosen.value.n}`:chosen.value?.net==='GND'?t("板卡公共地 → EX1 · PIN 2"):t("5 V 外设电源 → EX1 · PIN 1")):'');
</script>
<template>
 <section class="part-inspector" :class="{'is-header':part.kind==='header','is-cpu':id==='CPU'}" :aria-label="t('元件与引脚')">
  <header><div><small>{{part.ref}}</small><h2>{{part.name}}</h2></div><span class="part-supply">{{typeLabel}}</span></header>
  <div class="part-body" tabindex="0" :aria-label="en?'Pin details':'引脚详情'">
   <div v-if="id==='CPU'" class="cpu-summary"><p>ESP32-S3-WROOM-1U-N16R8</p><div class="cpu-memory"><div><strong>16 <small>MB</small></strong><span>Flash</span></div><div><strong>8 <small>MB</small></strong><span>PSRAM</span></div></div><p>{{en?'The main controller connects the board’s inputs, outputs and peripherals.':'主控制器负责连接板上的输入、输出与外设。'}}</p></div>
   <div v-if="!isSocket && ['IMU','RGB'].includes(id)" class="component-portrait" :class="'portrait-'+id">
    <svg v-if="id==='IMU'" viewBox="0 0 240 100" :aria-label="t('QMI8658A 惯性传感器示意')"><path d="M68 17h99l14 10v59H68z" fill="#b99b55"/><rect x="61" y="10" width="106" height="72" rx="5" fill="#273138"/><g fill="#d7ad58"><rect v-for="i in 5" :x="70+i*14" y="17" width="8" height="7" rx="1"/><rect v-for="i in 5" :x="70+i*14" y="68" width="8" height="7" rx="1"/></g><rect x="90" y="29" width="46" height="32" rx="3" fill="#151d24" stroke="#83919a"/><text x="113" y="48" text-anchor="middle" fill="#dbe4e8" font-size="8">QMI8658A</text><path d="M195 64V24m0 40h28m-28 0-18 15" fill="none" stroke="#008cd1" stroke-width="2"/><text x="200" y="26" fill="#008cd1" font-size="9">Z</text><text x="221" y="60" fill="#008cd1" font-size="9">X</text><text x="173" y="90" fill="#008cd1" font-size="9">Y</text></svg>
    <svg v-else-if="id==='RGB'" viewBox="0 0 240 100" :aria-label="t('RGB 贴片灯珠：白色封装、中央透光窗口和四个触点')"><g fill="#bca66c"><rect x="79" y="23" width="12" height="15" rx="1"/><rect x="79" y="58" width="12" height="15" rx="1"/><rect x="151" y="23" width="12" height="15" rx="1"/><rect x="151" y="58" width="12" height="15" rx="1"/></g><path d="M86 14h66l6 6v65H86z" fill="#d6cfbe"/><path d="M82 10h64l7 7v62H82z" fill="#f7f3e9" stroke="#b6b0a1"/><rect x="91" y="19" width="53" height="51" rx="9" fill="#a9b0a2" stroke="#8d9588"/><rect x="94" y="22" width="47" height="45" rx="7" :fill="running && output?.label!=='OFF' && output?.value>0?output.color:'#cbd0bd'"/><path d="M99 30l12 10m25-10-12 10m-25 19 12-8m25 8-12-8" fill="none" stroke="#b69c51" stroke-width="1"/><g fill="#6a7566"><rect x="110" y="37" width="7" height="7"/><rect x="120" y="37" width="7" height="7"/><rect x="115" y="48" width="7" height="7"/></g><path d="M82 69h8v10h-8" fill="#a99464"/><path d="M96 23h37" stroke="#ffffff88" stroke-width="3"/><text x="173" y="45" fill="#536777" font-size="10">RGB LED</text><text x="173" y="61" fill="#82939d" font-size="9">GPIO 45</text></svg>

   </div>
   <ConnectorDiagram v-if="isSocket" :part="part" :selected="pin" @pin="pin=$event"/>
   <p v-if="part.kind==='header'" class="orientation">{{t('正面 USB-C 朝下：第 1 行靠电源端，第 8 行靠 USB-C')}}</p>
   <div v-if="part.kind==='header'" class="header-pinout" role="group" :aria-label="t('EX2 排针：8 行，左中右三列')">
    <span class="column-title">{{t('行')}}</span><span v-for="label in [t('左列'),t('中列'),t('右列')]" :key="label" class="column-title">{{label}}</span>
    <template v-for="row in 8" :key="row"><span class="row-number">{{row}}</span><button v-for="p in pins.filter(p=>p.row===row)" :key="p.n" :aria-label="part.id+' '+p.n+' '+p.net" :aria-pressed="pin===p.n" @click="pin=p.n"><i :class="{power:p.gpio==null&&!p.net.includes('GND'),ground:p.net.includes('GND')}"></i><span>{{p.net}}</span></button></template>
   </div>
   <div v-if="!isSocket && part.kind!=='header'" class="connector-pins" :class="{socket:isSocket,header:part.kind==='header'}" :style="{'--count':pins.length}">
    <button v-for="p in pins" :key="p.n" :aria-label="part.id+' '+p.n+' '+p.net" :aria-pressed="pin===p.n" @click="pin=p.n" :class="{active:pin===p.n,power:p.gpio==null&&/V|5/.test(p.net),ground:p.net.includes('GND')}"><small>PIN {{p.n}}</small><strong>{{p.net}}</strong></button>
   </div>
   <div v-if="part.kind==='header'||chosen?.note||isSocket" class="pin-caption" aria-live="polite"><b>{{chosen?.net||part.name}}</b><span>{{chosen?.gpio!=null?[t('3.3 V 逻辑'),chosen.note].filter(Boolean).join(' · '):chosen?.note||part.note}}</span></div>


  </div>
   <div v-if="signal && flow" class="signal-summary" :class="{running}"><b>{{t('信号路径')}}</b><span>{{signal}}</span></div>
 </section>
</template>
<style scoped>
.part-inspector{background:white;border:1px solid #dce5eb;border-radius:12px;min-width:0;overflow:hidden}
.part-inspector header{display:flex;align-items:start;justify-content:space-between;gap:12px;padding:16px}
.part-inspector h2{font-size:18px;line-height:1.4;margin:2px 0 0;font-weight:650;overflow-wrap:anywhere}
.part-inspector header small{font-size:12px;color:#526575}.part-supply{font-size:12px;line-height:1.5;border-radius:6px;padding:4px 8px;background:#edf5fc;color:#42647d;flex-shrink:0}
.part-body{padding:0 16px 16px;min-width:0}.component-portrait{display:grid;place-items:center;min-height:100px;padding:8px 0 16px}
.component-portrait svg{width:240px;height:100px;max-width:100%}.generic-part{display:flex;flex-direction:column;gap:6px;padding:16px 24px;border:1px solid #bcc8cf;border-radius:8px;background:linear-gradient(135deg,#eef1ef,#cad2d5);font-size:16px}.generic-part small{font-size:12px;color:#526575}
.cpu-summary{display:grid;gap:16px}.cpu-summary p{font-size:12px;color:#647785;line-height:1.7;margin:0}.cpu-memory{display:grid;grid-template-columns:1fr 1fr;gap:12px}.cpu-memory>div{padding:14px 16px;background:#f5f8fc;border-radius:8px;display:grid;gap:2px}.cpu-memory strong{font:500 27px/1.2 Oxanium,sans-serif;color:#20384a}.cpu-memory strong small{font:12px Inter,sans-serif;color:#647785}.cpu-memory span{font-size:12px;color:#647785}
.orientation{font-size:12px;line-height:1.6;color:#526575;margin:0 0 12px!important}
.connector-pins{display:grid;grid-template-columns:repeat(auto-fit,minmax(100px,1fr));gap:8px}
.connector-pins button{display:flex;flex-direction:column;align-items:start;gap:4px;padding:10px 12px;border:1px solid #dce5eb;border-radius:8px;background:#f6f9fc;text-align:left;min-height:44px;min-width:0}
.connector-pins button small{font-size:12px;color:#526575}.connector-pins button strong{font-size:14px;color:#005aab;overflow-wrap:anywhere}
.connector-pins button.active{background:#edf5fc;border-color:#0076df}.connector-pins button.power strong{color:#75551e}.connector-pins button.ground strong{color:#526575}
.header-pinout{display:grid;grid-template-columns:20px repeat(3,minmax(0,1fr));gap:6px}
.header-pinout .column-title,.header-pinout .row-number{font-size:12px;color:#526575;display:grid;place-items:center}
.header-pinout button{display:flex;align-items:center;gap:6px;min-height:40px;min-width:0;padding:6px 8px;border:1px solid #dce5eb;border-radius:6px;font-size:13px;text-align:left;overflow-wrap:anywhere}
.header-pinout button[aria-pressed=true]{border-color:#0076df;background:#edf5fc;color:#005aab}
.header-pinout i{width:6px;height:6px;flex-shrink:0;background:#e7cf87;border:1px solid #b69751}.header-pinout i.power{background:#efd3ac}.header-pinout i.ground{background:#b9c4cc}
.pin-caption{display:flex;flex-wrap:wrap;gap:8px;margin-top:12px;font-size:12px;color:#526575;overflow-wrap:anywhere}.pin-caption b{color:#005aab}
.signal-summary{display:flex;flex-wrap:wrap;gap:8px;border-top:1px solid #dce5eb;background:#f6f9fc;padding:12px 16px;font-size:12px;color:#526575}.signal-summary b{font-weight:500}.signal-summary span{overflow-wrap:anywhere}
@media(max-width:600px){.header-pinout button{min-height:44px;padding:6px;font-size:12px}.part-inspector header{flex-wrap:wrap}}
</style>
