<script setup lang="ts">
import { withBase } from 'vitepress';
import { computed } from 'vue';
const props = withDefaults(defineProps<{angle?: string; labelled?: boolean; selected?: string; en?: boolean; ledColor?: string}>(), {angle:'front'});
const emit = defineEmits<{select: [name:string]}>();
const pins = [
 {name:'USB-C',pin:'USB · 5V',x:465,y:740,lx:240,ly:690},
 {name:'RGB LED',pin:'GPIO 45',x:521,y:1024,lx:280,ly:1190},
 {name:'PWM / ESC',pin:'GPIO 1 / 2',x:628,y:975,lx:610,ly:1190},
 {name:'Encoder',pin:'EA 3 · EB 9',x:711,y:975,lx:850,ly:1190},
 {name:'SBUS',pin:'RX 44',x:798,y:975,lx:1090,ly:1190},
 {name:'IMU',pin:'SDA 10 · SCL 11',x:976,y:503,lx:960,ly:285},
 {name:'ESP32-S3',pin:'N16R8',x:990,y:735,lx:1535,ly:665},
 {name:'Buzzer',pin:'GPIO 42',x:1210,y:740,lx:1535,ly:870},
];
const perspectivePoints = {
 'USB-C':[340,735], 'RGB LED':[568,948], 'PWM / ESC':[665,847],
 'Encoder':[750,797], 'SBUS':[820,752], 'IMU':[625,320],
 'ESP32-S3':[780,500], 'Buzzer':[978,390],
};
const hotspots = computed(()=>pins.map(p=>({ ...p, x:props.angle === 'angle' ? perspectivePoints[p.name][0] : p.x, y:props.angle === 'angle' ? perspectivePoints[p.name][1] : p.y })));
</script>
<template>
 <svg class="board-photo" :viewBox="labelled ? '80 200 1660 1080' : (angle === 'angle' || angle === 'back') ? '0 0 1448 1086' : '350 340 1100 800'" role="group" :aria-label="en ? 'OSRCORE board illustration' : 'OSRCORE 板卡插画示意'">
  <image v-if="!labelled && angle === 'angle'" :href="withBase('/hardware/board-illustration-angle.webp')" width="1448" height="1086" />
  <image v-else-if="!labelled && angle === 'back'" :href="withBase('/hardware/board-illustration-back.webp')" width="1448" height="1086" />
  <image v-else :href="withBase('/hardware/board-illustration-front.webp')" x="350" y="350" width="1100" height="759" />
  <image v-if="!labelled && angle === 'angle'" :href="withBase('/brand/icon-white.svg')" width="670" height="594" transform="matrix(0.065 -0.035 0.038 0.045 885 324)" />
  <ellipse v-if="ledColor && angle !== 'back'" :cx="!labelled && angle === 'angle' ? 568 : 521" :cy="!labelled && angle === 'angle' ? 948 : 1024" rx="14" ry="11" :fill="ledColor" :style="{filter:'drop-shadow(0 0 15px '+ledColor+')'}" />
  <g v-if="!labelled && angle !== 'back' && selected" class="board-hotspots">
    <g v-for="p in hotspots" :key="p.name" :class="{'active': selected === p.name}" @click="emit('select',p.name)" @keydown.enter="emit('select',p.name)" @keydown.space.prevent="emit('select',p.name)">
      <circle class="hit-area" role="button" tabindex="0" :aria-label="p.name+' · '+p.pin" :cx="p.x" :cy="p.y" r="28" />
      <circle class="hotspot-dot" :cx="p.x" :cy="p.y" r="8" />
      <g class="hotspot-caption"><rect :x="p.x-97" :y="p.y+24" width="194" height="48" rx="7"/><text :x="p.x" :y="p.y+57" text-anchor="middle">{{p.name}}</text></g>
    </g>
  </g>
  <g v-if="labelled" class="photo-labels">
   <g v-for="p in pins" :key="p.name" @click="emit('select',p.name)" @keydown.enter="emit('select',p.name)" @keydown.space.prevent="emit('select',p.name)" :class="{'is-active':selected===p.name}">
    <path :d="`M${p.x} ${p.y} L${p.lx} ${p.ly+28}`"/><circle :cx="p.x" :cy="p.y" r="8"/>
    <rect role="button" tabindex="0" :aria-label="p.name+' · '+p.pin" :x="p.lx-114" :y="p.ly-37" width="228" height="76" rx="10"/>
    <text :x="p.lx" :y="p.ly-5" text-anchor="middle">{{p.name}}</text><text class="pin-caption" :x="p.lx" :y="p.ly+22" text-anchor="middle">{{p.pin}}</text>
   </g>
  </g>
 </svg>
</template>
<style scoped>
.board-photo{width:100%;height:100%;overflow:visible;display:block}.photo-labels g{cursor:pointer;outline:none}.photo-labels path{fill:none;stroke:#6f8c9f;stroke-width:2}.photo-labels circle{fill:#0076df;stroke:white;stroke-width:3}.photo-labels rect{fill:#fff;stroke:#bfc9cf;stroke-width:1.5}.photo-labels text{pointer-events:none;font-family:Inter,sans-serif;font-size:33px;font-weight:600;fill:#101820}.photo-labels .pin-caption{font-size:27px;font-weight:400;fill:#536777}.photo-labels g:hover rect,.photo-labels g:focus rect,.photo-labels .is-active rect{stroke:#0076df;stroke-width:3;fill:#f1f8ff}
.board-hotspots .hit-area { fill:transparent; cursor:pointer; outline:none; }
.board-hotspots .hotspot-dot { fill:#fff; stroke:#0076df; stroke-width:4; pointer-events:none; }
.board-hotspots .hotspot-caption { opacity:0; pointer-events:none; }
.board-hotspots .hotspot-caption rect { fill:white; stroke:#bfc9cf; stroke-width:1.5; }
.board-hotspots .hotspot-caption text { fill:#101820; font:27px Inter,sans-serif; }
.board-hotspots g.active .hotspot-caption,.board-hotspots g:hover .hotspot-caption,.board-hotspots g:focus-within .hotspot-caption { opacity:1; }
.board-hotspots .hit-area:focus { stroke:#0076df; stroke-width:3; }
</style>
