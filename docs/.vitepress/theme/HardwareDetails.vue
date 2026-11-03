<script setup>
import {computed,ref,watch} from 'vue';
import {withBase} from 'vitepress';
import {X,ArrowRight,ExternalLink} from 'lucide-vue-next';
import {hardware,lessonGPIO} from './hardware-map.mjs';
const props=defineProps({id:String,lesson:Number,en:Boolean});const emit=defineEmits(['close','lesson','pin']);
const part=computed(()=>hardware.find(h=>h.id===props.id));const pin=ref('');
watch(()=>props.id,()=>{pin.value=part.value?.pins[0]?.n||''},{immediate:true});
watch(pin,value=>emit('pin',value),{immediate:true});
const chosen=computed(()=>part.value?.pins.find(p=>p.n===pin.value));
const ordered=computed(()=>part.value.reverse?[...part.value.pins].reverse():part.value.kind==='xt30'?[4,3,2,1].map(n=>part.value.pins.find(p=>p.n===String(n))):part.value.pins);
const category=p=>p.gpio!=null?'gpio':p.net.includes('GND')?'ground':/VCC|V_IN|5 V/.test(p.net)?'power':'signal';
const related=p=>p.gpio!=null&&(lessonGPIO[props.lesson]||[]).includes(p.gpio);
</script>
<template>
 <section v-if="part" class="hardware-details" aria-label="接口引脚详情">
  <header><div><small>{{part.side==='front'?'FRONT / 正面':'BACK / 背面'}}</small><h2>{{part.name}}</h2><p class="selected-pin-summary" v-if="chosen">{{chosen.n}} · {{chosen.net}} <span v-if="chosen.gpio!=null">/ 3.3 V</span></p></div><button @click="emit('close')" aria-label="关闭引脚详情"><X/></button></header>
  <div class="pin-scroll">
   <p class="pin-orientation">{{part.kind==='part'&&part.id!=='USBC'?'板载器件信号定义；不是外部插座线序。':part.orientation}}</p>
   <div v-if="part.kind==='header'" class="pin-column-labels"><span>行</span><span>左列</span><span>中列</span><span>右列</span></div>
   <div class="pin-layout" :class="'pin-layout-'+part.kind" :style="part.kind==='socket'?{'--pins':ordered.length}:{}">
    <template v-for="p in ordered" :key="p.n">
     <span v-if="part.kind==='header'&&p.col===0" class="pin-row-label">{{p.row}}</span>
     <button :class="[category(p),{active:pin===p.n,related:related(p)}]" :aria-label="part.id+' '+p.n+' '+p.net" :aria-pressed="pin===p.n" @click="pin=p.n" @mouseenter="pin=p.n" @focus="pin=p.n"><small>{{part.kind==='header'?p.n:(part.kind==='part'?p.n:'PIN '+p.n)}}</small><strong>{{p.net}}</strong><span v-if="related(p)">本课</span></button>
    </template>
   </div>
   <div v-if="chosen" class="pin-readout" aria-live="polite"><small>{{part.kind==='header'?'第 '+chosen.row+' 行 · '+['左列','中列','右列'][chosen.col]:'引脚 '+chosen.n}}</small><h3>{{chosen.net}}</h3><p v-if="chosen.gpio!=null">ESP32-S3 · GPIO {{chosen.gpio}} · 3.3 V 逻辑</p><p v-else-if="category(chosen)==='ground'">接地参考</p><p>{{chosen.note}}</p></div>
   <p v-if="part.note" class="hardware-note">{{part.note}}</p>
   <p v-if="part.kind==='header'" class="hardware-note">排针格中的 1L / 1M / 1R 是本页的行列坐标，不是连接器焊盘编号。左列 H6、中列 H5、右列 H4，各自从 1 到 8。</p>
   <button v-if="part.lesson!=null" class="hardware-lesson" @click="emit('lesson',part.lesson)">进入相关课程 <ArrowRight/></button>
   <footer><span>核对依据：原理图、PCB 丝印与产品手册</span><a :href="withBase('/hardware/schematic-zh.pdf')" target="_blank" rel="noopener">原理图 <ExternalLink/></a><a :href="withBase('/hardware/'+(part.side==='back'?'back':'front')+'.png')" target="_blank" rel="noopener">PCB 对照 <ExternalLink/></a></footer>
  </div>
 </section>
</template>
<style scoped>
.hardware-details{height:100%;min-height:0;display:flex;flex-direction:column;background:#fff;border-left:1px solid #e1e7ed;color:#182735}.hardware-details header{display:flex;justify-content:space-between;align-items:center;padding:20px 24px;border-bottom:1px solid #e1e7ed}.hardware-details header small{font-size:11px;letter-spacing:.1em;color:#0076df}.hardware-details h2{font-size:23px;font-weight:650}.hardware-details header button{padding:6px}.pin-scroll{overflow:auto;padding:20px 24px}.pin-orientation{font-size:13px;line-height:1.8;color:#536575;margin-bottom:16px!important}.pin-layout{display:grid;gap:7px;margin:12px 0 20px}.pin-layout-socket{grid-template-columns:repeat(var(--pins),minmax(0,1fr));overflow:auto}.pin-layout button{position:relative;border:1px solid #dbe2e8;background:#f5f7fa;border-radius:6px;padding:10px 4px;display:flex;flex-direction:column;align-items:center;gap:5px;min-width:0;line-height:1.4}.pin-layout button small{font-size:10px;color:#5e7280}.pin-layout button strong{font:600 12px Inter,sans-serif;overflow-wrap:anywhere}.pin-layout button.ground{background:#eff1f3}.pin-layout button.power{background:#fff3e7;color:#8c501a}.pin-layout button.gpio{color:#0063b8}.pin-layout button.active{border-color:#0076df;box-shadow:0 0 0 1px #0076df;background:#eaf4ff}.pin-layout button.related:after{content:'';position:absolute;width:5px;height:5px;background:#0076df;border-radius:50%;right:5px;top:5px}.pin-layout button>span{font-size:9px;color:#0076df}.pin-layout-header,.pin-column-labels{display:grid;grid-template-columns:20px repeat(3,minmax(0,1fr));gap:6px}.pin-layout-header button{padding:6px 2px;min-height:42px;flex-direction:row;justify-content:center;gap:6px}.pin-layout-header button>span{display:none}.pin-column-labels,.pin-row-label{font-size:11px;color:#6f7e8c;text-align:center;align-self:center}.pin-layout-xt30{grid-template-columns:1fr 1fr;max-width:280px;margin-left:auto;margin-right:auto}.pin-layout-xt30 button:nth-child(1),.pin-layout-xt30 button:nth-child(2){grid-column:1/3;width:70%;justify-self:center}.pin-layout-part{grid-template-columns:repeat(2,minmax(0,1fr))}.pin-readout{padding:15px 0;border-top:1px solid #e1e7ed;border-bottom:1px solid #e1e7ed;min-height:100px}.pin-readout small{font-size:12px;color:#607180}.pin-readout h3{font-size:24px;font-weight:600;margin:3px 0!important}.pin-readout p,.hardware-note{font-size:12px;line-height:1.8;color:#516575}.hardware-note{margin:14px 0!important}.hardware-lesson{display:flex;gap:10px;align-items:center;color:#0076df;font-size:13px}.hardware-details footer{margin-top:20px;font-size:11px;color:#70818e;display:flex;flex-wrap:wrap;gap:10px}.hardware-details footer span{width:100%}.hardware-details footer a{display:flex;gap:5px;align-items:center}.hardware-details footer svg{width:12px;height:12px}
.selected-pin-summary{font:13px Inter,sans-serif;color:#0076df;margin-top:6px!important}.selected-pin-summary span{color:#657888}</style>
