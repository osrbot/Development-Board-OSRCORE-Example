import {inject,ref} from 'vue';
import translations from './ui-translations.json';
export function useLanguage(){const en=inject('osrcore-language',ref(true));const t=text=>en.value?(translations[text]??text):text;return {en,t};}
