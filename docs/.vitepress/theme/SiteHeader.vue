<script setup>
import {withBase} from 'vitepress';
import {Home,BookOpen,Layers,CircleHelp,Languages,PlugZap,ArrowUpRight} from 'lucide-vue-next';
const props=defineProps({en:Boolean,docs:Boolean,deviceStatus:String,deviceBusy:Boolean,lesson:{type:Number,default:0},languageHref:String});
defineEmits(['board','lessons','help','language','connect']);
const home=()=>withBase(props.en?'/':'/zh/');
const tr=(zh,en)=>props.en?en:zh;
</script>
<template>
 <header class="experiment-header">
  <a class="experiment-brand" :href="home()" :aria-label="tr('OSRCORE 首页','OSRCORE home')"><img :src="withBase('/brand/logo.svg')" alt="OSRBOT"/><div><div class="site-title">OSRCORE <span>{{tr('入门','LEARN')}}</span></div></div></a>
  <nav class="lab-top-nav" :aria-label="tr('主菜单','Main navigation')">
   <component :is="docs?'a':'button'" :href="docs?home()+'#lesson-00':undefined" @click="$emit('board')"><Home/><span>{{tr('开发板','Controller')}}</span></component>
   <component :is="docs?'a':'button'" :href="docs?home()+'#lesson-'+String(lesson).padStart(2,'0'):undefined" @click="$emit('lessons')"><BookOpen/><span>{{tr('课程','Lessons')}}</span></component>
   <a :href="withBase(en?'/chapters.html':'/zh/chapters.html')"><Layers/><span>{{tr('资料','Resources')}}</span></a>
   <component :is="docs?'a':'button'" :href="docs?withBase(en?'/updater.html':'/zh/updater.html'):undefined" @click="$emit('help')"><CircleHelp/><span>{{tr('帮助','Help')}}</span></component>
   <component :is="docs?'a':'button'" :href="docs?languageHref:undefined" @click="$emit('language')" :aria-label="tr('切换语言','Switch language')"><Languages/><span>{{tr('语言','Language')}}</span></component>
  </nav>
  <a v-if="docs" class="connect-board" :href="home()+'#lesson-'+String(lesson).padStart(2,'0')"><ArrowUpRight/><span>{{tr('进入课程','Open lesson')}}</span></a>
  <button v-else class="connect-board" :disabled="deviceBusy" @click="$emit('connect')"><PlugZap/><span>{{deviceStatus||tr('连接开发板','Connect controller')}}</span></button>
 </header>
</template>
