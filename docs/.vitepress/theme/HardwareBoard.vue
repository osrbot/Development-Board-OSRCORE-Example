<script setup>
import {computed,ref,watch} from 'vue';
import {withBase} from 'vitepress';
import {hardware,lessonHardware} from './hardware-map.mjs';
import projection from './illustration-hotspots.json';
const props=defineProps({lesson:Number,selection:String,en:Boolean,ledColor:String,all:Boolean,pin:String});
const emit=defineEmits(['select']);
const side=ref('angle'),hover=ref(''),zoomed=ref(false);
const active=computed(()=>props.selection||lessonHardware[props.lesson]||'CPU');
const visible=computed(()=>hardware.filter(h=>h.side===(side.value==='back'?'back':'front')));
const point=h=>projection[side.value][h.id];
function go(id){zoomed.value=false;const h=hardware.find(h=>h.id===id);if(h.side==='back')side.value='back';else if(side.value==='back')side.value='angle';emit('select',id)}
watch(()=>props.lesson,()=>{const h=hardware.find(h=>h.id===lessonHardware[props.lesson]);side.value=h?.side==='back'?'back':'angle'});
watch(()=>props.selection,id=>{const h=hardware.find(h=>h.id===id);if(h&&h.side!==(side.value==='back'?'back':'front'))side.value=h.side==='back'?'back':'angle'});
</script>
<template>
 <div class="hardware-board" :class="{zoomed}" :style="{'--board-ratio':side==='angle'?1604/981:1330/1182}" @keydown.esc="zoomed=false">
  <div class="hardware-toolbar"><div class="hardware-views"><button v-for="v in [['angle','整体','Perspective'],['front','正面','Front'],['back','背面','Back']]" :key="v[0]" :aria-pressed="side===v[0]" @click="side=v[0]">{{en?v[2]:v[1]}}</button></div><button class="board-zoom" @click="zoomed=!zoomed" :aria-label="zoomed?'收起板卡':'放大板卡'">{{zoomed?'收起':'放大'}}</button><span>{{en?'Select a connector to inspect its pins':'点击接口，展开线序与 GPIO'}}</span></div>
  <div class="hardware-viewport"><div class="hardware-art">
   <img :src="withBase('/hardware/board-styled-v2-'+side+'.webp')" :alt="en?'OSRCORE interactive board illustration':`OSRCORE ${side==='back'?'背面':'正面'} · 交互插画`" draggable="false"/>
   <button v-for="h in visible" :key="h.id" class="hardware-hotspot" :class="{selected:active===h.id,reveal:all||hover===h.id}" :style="{left:point(h)[0]*100+'%',top:point(h)[1]*100+'%'}" :aria-label="'查看 '+h.name+' 引脚'" :aria-pressed="selection===h.id" @click="go(h.id)" @mouseenter="hover=h.id" @mouseleave="hover=''" @focus="hover=h.id" @blur="hover=''">
    <i :style="h.id==='RGB'&&ledColor?{background:ledColor,boxShadow:'0 0 18px '+ledColor}:{}"></i><span>{{h.name}}</span>
   </button>
   <span v-if="side==='front' && selection==='EX2' && pin && projection[side]['EX2-'+pin]" class="physical-pin" :style="{left:projection[side]['EX2-'+pin][0]*100+'%',top:projection[side]['EX2-'+pin][1]*100+'%'}"></span>
  </div>
  </div>
  <div class="hardware-selector"><label>{{en?'Find connector':'查找接口'}}<select :value="selection||''" @change="go($event.target.value)" aria-label="查找板卡接口"><option value="" disabled>{{en?'Select connector / component':'选择端子 / 排针 / 功能区'}}</option><optgroup v-for="s in ['front','back']" :key="s" :label="s==='front'?'正面 · Front':'背面 · Back'"><option v-for="h in hardware.filter(h=>h.side===s)" :key="h.id" :value="h.id">{{h.name}}</option></optgroup></select></label><small>{{side==='back'?'背面俯视 · USB-C 边缘在下':'正面方位 · USB-C 位于板卡底边'}}</small></div>
 </div>
</template>
<style scoped>
.hardware-board{position:relative;height:100%;display:flex;flex-direction:column;min-height:0}.hardware-toolbar{display:flex;justify-content:space-between;align-items:center;gap:10px;font-size:12px;color:#647383}.hardware-views{display:flex;gap:4px}.hardware-views button{padding:4px 12px;min-height:34px;border-radius:5px}.hardware-views button[aria-pressed=true]{background:#e7f2ff;color:#0076df}.hardware-viewport{flex:1;min-height:0;width:100%;display:grid;place-items:center;container-type:size}.hardware-art{position:relative;width:min(100cqw,calc(100cqh * var(--board-ratio)));height:auto;aspect-ratio:var(--board-ratio);isolation:isolate}.hardware-art img{width:100%;height:100%;object-fit:contain;display:block;filter:drop-shadow(0 16px 14px #26344916)}
.hardware-hotspot{position:absolute;transform:translate(-50%,-50%);display:grid;place-items:center;width:30px;height:30px;min-height:30px!important;padding:0;border-radius:50%;z-index:1}.hardware-hotspot i{width:5px;height:5px;background:#fff;border:1.5px solid #0076df;border-radius:50%;box-shadow:0 0 0 2px #0076df18;pointer-events:none}.hardware-hotspot span{position:absolute;bottom:30px;left:50%;transform:translateX(-50%);background:#fff;padding:4px 8px;border:1px solid #d6e1eb;border-radius:4px;white-space:nowrap;opacity:0;pointer-events:none;font-size:12px;color:#182735}.hardware-hotspot.selected,.hardware-hotspot:hover,.hardware-hotspot:focus-visible{z-index:3}.hardware-hotspot.selected i{width:10px;height:10px;background:#0076df;box-shadow:0 0 0 5px #0076df25}.hardware-hotspot.selected span,.hardware-hotspot.reveal span{opacity:1}.hardware-selector{display:flex;align-items:center;justify-content:space-between;gap:10px;font-size:12px;color:#607180;padding:8px 0}.hardware-selector label{display:flex;align-items:center;gap:10px}.hardware-selector select{max-width:240px;background:white;border:1px solid #d5dee8;border-radius:5px;padding:8px;color:#243849}.hardware-selector small{font-size:11px}
@media(max-width:850px){.hardware-toolbar>span{display:none}.hardware-viewport{flex:none;aspect-ratio:var(--board-ratio);container-type:inline-size}.hardware-art{width:100%;height:auto}.hardware-selector{flex-wrap:wrap}.hardware-selector select{max-width:230px}}

@media(min-width:851px){.hardware-selector{position:absolute;right:0;top:0;padding:0;z-index:5}.hardware-selector label{font-size:0}.hardware-selector select{font-size:11px;width:185px;padding:7px}.hardware-selector small{display:none}}
.physical-pin{position:absolute;transform:translate(-50%,-50%);width:10px;height:10px;border:2px solid white;border-radius:50%;background:#ef8d23;box-shadow:0 0 0 4px #ef8d2355;pointer-events:none;z-index:5}

.board-zoom{font-size:12px;color:#0076df;min-height:32px!important;padding:0 8px;margin-right:auto}.hardware-board.zoomed{position:fixed;inset:20px;width:calc(100vw - 40px)!important;height:calc(100dvh - 40px)!important;z-index:60;background:#fcfdfe;padding:24px;border:1px solid #d6e1eb;border-radius:10px;box-shadow:0 0 0 40px #14263f66}.hardware-board.zoomed .hardware-selector{top:24px;right:24px}.hardware-board.zoomed .hardware-toolbar>span{display:none}
</style>

<style scoped>
.hardware-board.zoomed .hardware-viewport{flex:1;min-height:0;aspect-ratio:auto;container-type:size}.hardware-board.zoomed .hardware-art{width:min(100cqw,calc(100cqh * var(--board-ratio)))}
</style>
