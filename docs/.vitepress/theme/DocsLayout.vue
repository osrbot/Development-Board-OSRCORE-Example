<script setup>
import {computed,ref,watch,nextTick,onMounted,onUnmounted} from 'vue';
import {useData,withBase} from 'vitepress';
import DefaultTheme from 'vitepress/theme';
import SiteHeader from './SiteHeader.vue';
const {page}=useData();
const header=ref(null),headerHeight=ref(80);
let observer;
async function measureHeader(){
 observer?.disconnect();
 await nextTick();
 if(!header.value?.$el)return;
 observer=new ResizeObserver(([entry])=>headerHeight.value=entry.target.getBoundingClientRect().height);
 observer.observe(header.value.$el);
}
onMounted(measureHeader);
onUnmounted(()=>observer?.disconnect());
const isDoc=computed(()=>!['index.md','zh/index.md'].includes(page.value.relativePath));
watch(isDoc,measureHeader,{flush:'post'});
const en=computed(()=>!page.value.relativePath.startsWith('zh/'));
const lesson=computed(()=>Number(page.value.relativePath.match(/chapter-(\d+)/)?.[1]||0));
const languageHref=computed(()=>withBase((en.value?'/zh/':'/')+page.value.relativePath.replace(/^zh\//,'').replace(/\.md$/,'.html')));
</script>
<template><div :class="{'docs-shell':isDoc}" :style="{'--docs-header-height':headerHeight+'px'}"><SiteHeader ref="header" v-if="isDoc" class="docs-site-header" :docs="true" :en="en" :lesson="lesson" :language-href="languageHref"/><DefaultTheme.Layout/></div></template>
