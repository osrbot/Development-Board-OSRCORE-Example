<script setup>
import {useLanguage} from './language.mjs';
const {en,t}=useLanguage();
import {computed} from 'vue';
const props=defineProps({part:Object,selected:String});
const emit=defineEmits(['pin']);
const pins=computed(()=>props.part.reverse?[...props.part.pins].reverse():props.part.pins);
const width=computed(()=>Math.max(240,pins.value.length*52+32));
const x=i=>32+(width.value-64)*(i+.5)/pins.value.length;
const xtPositions={'1':[116,116],'2':[64,116],'3':[90,78],'4':[90,38]};
</script>
<template>
 <div class="terminal-layout" :class="{many:pins.length>4}">
  <figure>
   <svg v-if="part.kind==='socket'" :viewBox="`0 0 ${width} 172`" role="group" :aria-label="part.name+' 插座开口正视图'">
    <rect x="19" y="22" :width="width-38" height="57" class="shell"/>
    <rect x="27" y="30" :width="width-54" height="40" class="opening"/>
    <path :d="`M14 83H${width-14}v5H14Z`" class="base"/>
    <g v-for="(p,i) in pins" :key="p.n" class="pin-target" :class="{selected:p.n===selected}" role="button" tabindex="0" :aria-label="part.id+' 引脚 '+p.n+' '+p.net" @click="emit('pin',p.n)" @keydown.enter.prevent="emit('pin',p.n)" @keydown.space.prevent="emit('pin',p.n)">
     <rect :x="x(i)-23" y="34" width="46" height="116" fill="transparent"/>
     <rect :x="x(i)-3" y="43" width="6" height="18" class="contact"/>
     <path :d="`M${x(i)} 62V111`" class="leader"/>
     <text :x="x(i)" y="128" class="number">{{p.n}}</text>
     <text :x="x(i)" y="146" class="net">{{p.net}}</text>
    </g>
   </svg>
   <svg v-else-if="part.kind==='xt30'" viewBox="0 0 240 180" role="group" :aria-label="part.name+' 正面俯视引脚图'">
    <path d="M48 12H132V136H48Z" class="shell"/><path d="M55 19H125V129H55Z" class="opening"/>
    <g v-for="p in pins" :key="p.n" class="pin-target" :class="{selected:p.n===selected}" role="button" tabindex="0" :aria-label="part.id+' 引脚 '+p.n+' '+p.net" @click="emit('pin',p.n)" @keydown.enter.prevent="emit('pin',p.n)" @keydown.space.prevent="emit('pin',p.n)">
     <circle v-if="Number(p.n)>2" :cx="xtPositions[p.n][0]" :cy="xtPositions[p.n][1]" r="12" class="contact"/>
     <rect v-else :x="xtPositions[p.n][0]-4" y="112" width="8" height="8" class="contact"/>
     <path :d="Number(p.n)>2?`M102 ${xtPositions[p.n][1]}H153`:`M${xtPositions[p.n][0]} 121V147`" class="leader"/>
     <text v-if="Number(p.n)>2" x="160" :y="xtPositions[p.n][1]+4" class="net" text-anchor="start">{{p.n}} · {{p.net}}</text>
     <template v-else><text :x="xtPositions[p.n][0]" y="159" class="number">{{p.n}}</text><text :x="xtPositions[p.n][0]" y="174" class="net">{{p.net}}</text></template>
    </g>
   </svg>
   <figcaption>{{part.kind==='xt30'?t('正面俯视 · 电源端朝上'):t('插座开口正视 · PCB 在下')}}</figcaption>
  </figure>
  <div v-if="pins.length<=4" class="pin-table" role="group" :aria-label="t('引脚信号表')">
   <div class="table-heading"><span>{{t('引脚')}}</span><span>{{t('信号')}}</span><template v-if="pins.length>4"><span>{{t('引脚')}}</span><span>{{t('信号')}}</span></template></div>
   <button v-for="p in part.pins" :key="p.n" :aria-label="part.id+' '+p.n+' '+p.net" :aria-pressed="selected===p.n" @click="emit('pin',p.n)"><span>{{p.n}}</span><strong>{{p.net}}</strong></button>
  </div>
 </div>
</template>
<style scoped>
.terminal-layout{display:grid;grid-template-columns:minmax(0,1.35fr) minmax(128px,1fr);align-items:center;gap:20px;padding:16px 0 8px}.terminal-layout figure{margin:0;min-width:0}.terminal-layout svg{display:block;width:100%;height:auto;max-height:160px;overflow:visible}.shell{fill:#f1f4f6;stroke:#647582;stroke-width:1.5}.opening{fill:white;stroke:#94a2ac;stroke-width:1}.base{fill:#edf1f4;stroke:#9eabb4;stroke-width:1}.contact{fill:#dbe3e9;stroke:#728693;stroke-width:1}.leader{fill:none;stroke:#92a4b2;stroke-width:1}.number{fill:#0086d7;font:500 12px sans-serif;text-anchor:middle}.net{fill:#354956;font:10px sans-serif;text-anchor:middle}.net[text-anchor=start]{text-anchor:start}.pin-target{cursor:pointer;outline:none}.pin-target.selected .contact,.pin-target:hover .contact,.pin-target:focus-visible .contact{fill:#0096df;stroke:#007bbf}.pin-target.selected .leader,.pin-target:hover .leader{stroke:#008bd2;stroke-width:1.8}.pin-target.selected .net{fill:#007bbf;font-weight:700}figcaption{text-align:center;font-size:12px;color:#7b8b96;margin-top:5px}.pin-table{border:1px solid #d8e1e7;border-radius:5px;overflow:hidden;font-size:13px}.table-heading,.pin-table button{display:grid;grid-template-columns:40px 1fr;width:100%;text-align:left;align-items:center;min-height:40px;padding:0 10px;gap:5px}.table-heading{background:#f2f6f8;color:#6e818f;font-size:12px}.pin-table button{border-top:1px solid #e5ebef;color:#324b5c}.pin-table button strong{font-weight:500}.pin-table button[aria-pressed=true]{background:#e8f5fc;color:#007cbd;box-shadow:inset 3px 0 #008cd4}.pin-table button:hover{background:#f0f8fd}.pin-table button:focus-visible{outline:2px solid #008cd4;outline-offset:-2px}.terminal-layout.many{grid-template-columns:1fr;gap:8px;padding-top:8px}.many svg{max-height:168px}.many .pin-table{display:grid;grid-template-columns:1fr 1fr}.many .table-heading{grid-column:1/-1;grid-template-columns:40px 1fr 40px 1fr}.many .pin-table button:nth-child(even){border-right:1px solid #e5ebef}.many figcaption{margin-top:0}@media(max-width:430px){.terminal-layout{grid-template-columns:1fr;gap:10px}.terminal-layout svg{max-height:160px}}
</style>
