<script setup>
import { ref, computed, watch, onMounted, onUnmounted, nextTick, provide } from "vue";
import { withBase } from "vitepress";
import {
  Home,
  BookOpen,
  Cpu,
  Play,
  Square,
  Settings2,
  Languages,
  X,
  ChevronRight,
  ChevronLeft,
  Copy,
  Check,
  Maximize2,
  Minimize2,
  Download,
  Lightbulb,
  Volume2,
  VolumeX,
  SlidersHorizontal,
  Radio,
  Compass,
  Gauge,
  HardDrive,
  Layers,
  Activity,
  Navigation,
  Magnet,
  Bot,
  PlugZap,
  RotateCcw,
  Save,
  ArrowUpRight,
} from "lucide-vue-next";
import ExploreBoard from "./ExploreBoard.vue";
import PartInspector from "./PartInspector.vue";
import DeviceLab from "./DeviceLab.vue";
import {lessonGuide, lessonHardwareEn, firmwareParameters, firmwareValues, configHeader} from "./lesson-contract.mjs";
import { hardware, lessonHardware } from "./hardware-map.mjs";
import sourceLessons from "./courses.json";
import {
  lessons,
  initialValues,
  createState,
  lessonCode,
  stepSimulation,
} from "./lab-simulation.mjs";
import "./interactive.css";
const props = defineProps({ locale: String });
const en = ref(props.locale !== "zh");
provide("osrcore-language",en);
const tr = (zh, eng) => (en.value ? eng : zh);
const hardwareSelection = ref("");
const hardwarePin = ref("");
watch(hardwareSelection, () => { expanded.value = false; hardwarePin.value=""; });
const activeHardware = computed(()=>hardwareSelection.value||lessonHardware[selected.value]||'CPU');
const selectedContact = computed(()=>hardware.find(h=>h.id===activeHardware.value)?.pins.find(p=>p.n===hardwarePin.value));
function selectHardware(id){
 const h=hardware.find(p=>p.id===id);if(!h)return;
 stop();
 if(id==='EX1'&&selected.value!==0)choose(0);
 else if(h.lesson!=null&&h.lesson!==selected.value)choose(h.lesson);
 hardwareSelection.value=id;
 output.value=null;logs.value=[];showSource.value=false;
}

const deviceOpen=ref(false), deviceStatus=ref('');
const selected = ref(0),
  values = ref(initialValues(0)),
  running = ref(false),
  elapsed = ref(0),
  output = ref(null),
  logs = ref([]),
  inspector = ref(false),
  sidebar = ref(false),
  settings = ref(false),
  infoTab = ref("overview"),
  expanded = ref(false),
  showSource = ref(false),
  copied = ref(false),
  source = ref(null),
  sourceError = ref(false),
  sourceIndex = ref(0),
  audioEnabled = ref(false),
  progress = ref([]),
  flow = ref(true),
  graph = ref(),
  codeLines = ref(),
  courseRow = ref();
const icons = [
  Cpu,
  Lightbulb,
  Volume2,
  SlidersHorizontal,
  Radio,
  Compass,
  Gauge,
  HardDrive,
  Layers,
  Activity,
  Navigation,
  Magnet,
  Bot,
  Check,
];

const lesson = computed(() => lessons[selected.value]);
const guide=computed(()=>en.value?{goal:lesson.value.en,action:lesson.value.descriptionEn,result:lesson.value.descriptionEn,hardware:lessonHardwareEn[selected.value]}:lessonGuide[selected.value]);
const parameterHeader=computed(()=>configHeader(selected.value,firmwareValues(selected.value,values.value)));
const code = computed(() => {
 if(activeHardware.value!=='EX1')return lessonCode(selected.value, values.value).map(line=>en.value?line.replace('教学节选','Teaching excerpt'):line);
 const gpio=selectedContact.value?.gpio;
 if(gpio==null)return [tr('// 此针脚为供电或接地，没有 GPIO 控制代码。','// Power or ground pin; no GPIO control code.'),tr('// 请先选择 EX1 的 GPIO 针脚。','// Select an EX1 GPIO pin.')];
 return [tr('// EX1 · GPIO 输出教学示例','// EX1 · GPIO output example'),`const gpio_num_t pin = GPIO_NUM_${gpio};`,'gpio_reset_pin(pin);','gpio_set_direction(pin, GPIO_MODE_OUTPUT);','while (1) {','  gpio_set_level(pin, 1);','  vTaskDelay(pdMS_TO_TICKS(500));','  gpio_set_level(pin, 0);','  vTaskDelay(pdMS_TO_TICKS(500));','}'];
});
const files = computed(
  () =>
    sourceLessons[selected.value]?.files ||
    (selected.value === 13
      ? [{ name: "main.c", url: "/code/13_factory_demo/main.c.json" }]
      : []),
);
let state = createState(),
  timer,
  started = 0,
  last = 0,
  lastLog = "",
  lastLogAt = -1000,
  history = [],
  audio,
  osc,
  gain,
  loadToken = 0,
  scrollFrame = 0;
function addLog(text) {
  if (!text) return;
  logs.value = [
    ...logs.value.slice(-29),
    { time: (elapsed.value / 1000).toFixed(2), text },
  ];
}
function paint() {
  const canvas = graph.value;
  if (!canvas) return;
  const ctx = canvas.getContext("2d"),
    w = canvas.width,
    h = canvas.height;
  ctx.clearRect(0, 0, w, h);
  if (!history.length) return;
  let max = Math.max(1, ...history.map(Math.abs));
  ctx.strokeStyle = "#d6dee3";
  ctx.lineWidth = 1;
  for (let i = 1; i < 4; i++) {
    ctx.beginPath();
    ctx.moveTo(0, (h * i) / 4);
    ctx.lineTo(w, (h * i) / 4);
    ctx.stroke();
  }
  ctx.strokeStyle = "#0076df";
  ctx.lineWidth = 2;
  ctx.beginPath();
  history.forEach((v, i) => {
    const x = (i / Math.max(1, history.length - 1)) * w,
      y = h * 0.5 - (v / max) * h * 0.43;
    i ? ctx.lineTo(x, y) : ctx.moveTo(x, y);
  });
  ctx.stroke();
}
function updateAudio() {
  if (!audio || !osc || !gain) return;
  const audible =
    running.value &&
    selected.value === 2 &&
    audioEnabled.value &&
    output.value?.value > 0;
  osc.frequency.setTargetAtTime(
    Math.max(1, output.value?.value || 440),
    audio.currentTime,
    0.015,
  );
  gain.gain.setTargetAtTime(audible ? 0.025 : 0, audio.currentTime, 0.015);
}
function sample() {
  const now = performance.now();
  elapsed.value = now - started;
  output.value = stepSimulation(
    selected.value,
    values.value,
    elapsed.value,
    Math.min(0.1, (now - last) / 1000),
    state,
  );
  if(activeHardware.value==='EX1'){
   const g=selectedContact.value?.gpio,high=Math.floor(elapsed.value/500)%2===0;
   output.value={value:g==null?0:Number(high),label:g==null?'供电 / 地':high?'HIGH':'LOW',unit:'',secondary:g==null?'请选择 GPIO 针脚':`GPIO${g} · ${high?'3.3 V':'0 V'}`,line:high?5:7,log:g==null?'电源和接地针脚不执行 GPIO 输出':`EX1 · GPIO${g} → ${high?'HIGH':'LOW'}`};
  }
  last = now;
  history.push(output.value.value);
  if (history.length > 100) history.shift();
  paint();
  updateAudio();
  if (
    elapsed.value - lastLogAt > 350 &&
    output.value.log &&
    (output.value.log !== lastLog || elapsed.value - lastLogAt > 1200)
  ) {
    addLog(output.value.log);
    lastLog = output.value.log;
    lastLogAt = elapsed.value;
  }
  if (activeHardware.value!=='EX1' && elapsed.value > 1800 && !progress.value.includes(selected.value)) {
    progress.value.push(selected.value);
    persist();
  }
}
function stop() {
  running.value = false;
  clearInterval(timer);
  timer = null;
  updateAudio();
}
function run() {
  if (running.value) {
    stop();
    addLog(tr("已停止", "Stopped"));
    return;
  }
  if(activeHardware.value==='EX1'&&selectedContact.value?.gpio==null){addLog('请选择 GPIO 针脚后运行');return;}
  state = createState();
  if (selected.value === 7) {
    try {
      state.saved = JSON.parse(
        localStorage.getItem("osrcore-lab-nvs") || "null",
      );
    } catch {}
  }
  history = [];
  logs.value = [];
  elapsed.value = 0;
  lastLog = "";
  lastLogAt = -1000;
  started = last = performance.now();
  running.value = true;
  showSource.value = false;
  addLog(tr("教学模拟已启动", "Lesson simulation started"));
  sample();
  timer = setInterval(sample, 50);
}
function persist() {
  try {
    localStorage.setItem(
      "osrcore-interactive-progress",
      JSON.stringify(progress.value),
    );
  } catch {}
}
function choose(i) {
  stop();
  hardwareSelection.value="";
  selected.value = i;
  values.value = initialValues(i);
  state = createState();
  output.value = null;
  elapsed.value = 0;
  logs.value = [];
  history = [];
  sourceIndex.value = 0;
  showSource.value = false;
  inspector.value = false;
  sidebar.value = false;
  infoTab.value = "overview";
  if (typeof window !== "undefined") historyReplace(i);
  nextTick(() => {
    paint();
    const el = courseRow.value?.children[i];
    if (el && courseRow.value) {
      courseRow.value.scrollTo({
        left: Math.max(0, el.offsetLeft - courseRow.value.offsetLeft - 16),
        behavior: "smooth",
      });
    }
  });
  loadSource();
}
function historyReplace(i) {
  window.history.replaceState(
    null,
    "",
    "#lesson-" + String(i).padStart(2, "0"),
  );
}
async function loadSource() {
  source.value = null;
  sourceError.value = false;
  const token = ++loadToken;
  const file = files.value[sourceIndex.value];
  if (!file) return;
  try {
    const response = await fetch(withBase(file.url));
    if (!response.ok) throw Error();
    const data = await response.json();
    if (token === loadToken) source.value = data;
  } catch {
    if (token === loadToken) sourceError.value = true;
  }
}
watch(sourceIndex, loadSource);
watch(
  values,
  () => {
    if (running.value) sample();
    else if (output.value) {
      output.value = stepSimulation(selected.value, values.value, 0, 0, state);
      paint();
    }
  },
  { deep: true },
);
watch(
  () => output.value?.line,
  () => {
    if (!running.value || showSource.value) return;
    cancelAnimationFrame(scrollFrame);
    scrollFrame = requestAnimationFrame(() => {
      const target = codeLines.value?.children[output.value.line];
      if (target && codeLines.value) {
        const parent = codeLines.value;
        const y = target.offsetTop - parent.offsetTop;
        if (
          y < parent.scrollTop ||
          y + 30 > parent.scrollTop + parent.clientHeight
        )
          parent.scrollTop = Math.max(0, y - 70);
      }
    });
  },
);
async function enableAudio() {
  audioEnabled.value = !audioEnabled.value;
  if (audioEnabled.value) {
    try {
      audio ||= new (window.AudioContext || window.webkitAudioContext)();
      if (!osc) {
        osc = audio.createOscillator();
        gain = audio.createGain();
        gain.gain.value = 0;
        osc.connect(gain);
        gain.connect(audio.destination);
        osc.start();
      }
      await audio.resume();
    } catch {
      audioEnabled.value = false;
      addLog(tr("浏览器未允许播放声音", "Audio unavailable"));
    }
  }
  updateAudio();
}
async function copy() {
  const text = showSource.value ? parameterHeader.value+(source.value?.code||'') : code.value.join("\n");
  if (!text) return;
  try {
    if (navigator.clipboard && window.isSecureContext)
      await navigator.clipboard.writeText(text);
    else {
      const area = document.createElement("textarea");
      area.value = text;
      area.style.position = "fixed";
      area.style.opacity = "0";
      document.body.append(area);
      area.select();
      const ok = document.execCommand("copy");
      area.remove();
      if (!ok) throw Error();
    }
    copied.value = true;
    setTimeout(() => (copied.value = false), 1500);
  } catch {
    addLog(
      tr("复制失败，请手动选择代码", "Copy failed; select the code manually"),
    );
  }
}
function download() {
  if (!source.value) return;
  const url = URL.createObjectURL(
    new Blob([parameterHeader.value,source.value.code], { type: "text/plain" }),
  );
  const a = document.createElement("a");
  a.href = url;
  a.download = files.value[sourceIndex.value].name;
  a.click();
  URL.revokeObjectURL(url);
}
function saveNvs() {
  try {
    localStorage.setItem("osrcore-lab-nvs", JSON.stringify(values.value.kp));
    state.saved = values.value.kp;
    addLog(`NVS saved kp=${state.saved}`);
    output.value = stepSimulation(7, values.value, elapsed.value, 0, state);
  } catch {
    addLog(tr("浏览器存储不可用", "Browser storage unavailable"));
  }
}
function restartNvs() {
  try {
    const saved = JSON.parse(localStorage.getItem("osrcore-lab-nvs") || "null");
    if (saved !== null) {
      values.value.kp = saved;
      state.saved = saved;
      addLog(`NVS restored kp=${saved}`);
    } else addLog(tr("还没有保存值", "No saved value"));
  } catch {
    addLog(tr("读取失败", "Read failed"));
  }
}
function keydown(e) {
  if (e.target.closest("input,select,textarea,button,a")) return;
  if (e.code === "Space") {
    e.preventDefault();
    run();
  }
  if (e.key === "Escape") {
    inspector.value = false;
    settings.value = false;
    expanded.value = false;
  }
  if (e.key === "ArrowRight") choose(Math.min(13, selected.value + 1));
  if (e.key === "ArrowLeft") choose(Math.max(0, selected.value - 1));
}
function hashchange() {
  const n = Number(location.hash.match(/lesson-(\d+)/)?.[1] || 0);
  if (n >= 0 && n < lessons.length && n !== selected.value) choose(n);
}
function visibility() {
  if (document.hidden && running.value) {
    stop();
    addLog(tr("页面隐藏，模拟已暂停", "Page hidden; simulation paused"));
  }
}
onMounted(() => {
  try {
    const saved = JSON.parse(
      localStorage.getItem("osrcore-interactive-progress") || "[]",
    );
    progress.value = Array.isArray(saved)
      ? saved.filter((n) => Number.isInteger(n) && n >= 0 && n < lessons.length)
      : [];
  } catch {}
  hashchange();
  loadSource();
  window.addEventListener("keydown", keydown);
  window.addEventListener("hashchange", hashchange);
  document.addEventListener("visibilitychange", visibility);
});
onUnmounted(() => {
  stop();
  cancelAnimationFrame(scrollFrame);
  audio?.close();
  window.removeEventListener("keydown", keydown);
  window.removeEventListener("hashchange", hashchange);
  document.removeEventListener("visibilitychange", visibility);
});
const displayValue = computed(() =>
  output.value
    ? Number(output.value.value).toFixed(
        [5, 9, 10, 12].includes(selected.value) ? 2 : 0,
      )
    : "—",
);
</script>
<template>
  <div
    class="interactive-lab hardware-lab"
    :class="{ 'nav-open': sidebar, 'wide-code': expanded }"
  >
    <DeviceLab :open="deviceOpen" :lesson="selected" :values="values" @close="deviceOpen=false" @status="deviceStatus=$event" />
    <header class="experiment-header">
      <div class="experiment-brand">
        <img
          :src="withBase('/brand/logo.svg')"
          alt="OSRBOT"
          @error="$event.target.style.display = 'none'"
        />
        <div>
          <h1>
            OSRCORE <span>{{ tr("入门", "LEARN") }}</span>
          </h1>
          <p>{{ tr("探索 · 理解 · 动手", "EXPLORE · UNDERSTAND · BUILD") }}</p>
        </div>
      </div>
      <nav class="lab-top-nav" :aria-label="tr('主菜单', 'Main navigation')">
       <button @click="choose(0)" :aria-label="tr('认识开发板', 'Meet the controller')"><Home/><span>{{tr('开发板','Controller')}}</span></button>
       <button @click="sidebar=!sidebar" :aria-expanded="sidebar"><BookOpen/><span>{{tr('课程','Lessons')}}</span></button>
       <a :href="withBase(en?'/chapters.html':'/zh/chapters.html')"><Layers/><span>{{tr('资料','Resources')}}</span></a>
       <button @click="settings=!settings" :aria-label="tr('设置', 'Settings')"><Settings2/><span>{{tr('设置','Settings')}}</span></button>
       <button @click="en=!en" :aria-label="tr('切换语言', 'Switch language')"><Languages/><span>{{tr('语言','Language')}}</span></button>
      </nav>
      <button class="connect-board" @click="deviceOpen=true"><PlugZap/><span>{{!deviceStatus?tr('连接开发板','Connect controller'):deviceStatus}}</span></button>
      <div class="experiment-progress">
        <BookOpen />
        <div>
          <strong>{{ tr("学习进度", "PROGRESS") }}</strong
          ><progress
            :value="progress.length"
            :max="lessons.length"
            :aria-label="tr('学习进度', 'Learning progress')"
          ></progress>
        </div>
        <b
          >{{ progress.length }}<small>/{{ lessons.length }}</small></b
        >
      </div>
    </header>
    <div v-if="sidebar" class="lesson-drawer">
      <header>
        <strong>{{ tr("全部课程", "All lessons") }}</strong
        ><button @click="sidebar = false" :aria-label="tr('关闭', 'Close')">
          <X />
        </button>
      </header>
      <button
        v-for="l in lessons"
        :key="l.id"
        :class="{ current: selected === l.id }"
        @click="choose(l.id)"
      >
        <span>{{ String(l.id).padStart(2, "0") }}</span
        >{{ en ? l.en : l.zh }}<Check v-if="progress.includes(l.id)" />
      </button>
    </div>
    <main
      class="experiment-stage"
      :aria-label="tr('交互实验台', 'Interactive workbench')"
    >
      <section class="experiment-info"><div class="info-content"><span class="lesson-eyebrow">{{tr('交互实验台','INTERACTIVE WORKBENCH')}}</span><h2>{{activeHardware==='EX1'?tr('EX1 · GPIO 实验','EX1 · GPIO lab'):en?lesson.en:lesson.zh}}</h2><p>{{en?lesson.descriptionEn:guide.action}}</p></div></section>
      <details class="lesson-guide"><summary>{{tr('本课目标与接线','Goal & wiring')}}</summary><strong>{{guide.goal}}</strong><p>{{guide.hardware}}</p><p>{{tr('观察结果：','Expected result: ')}}{{guide.result}}</p></details>
      <div class="new-board-stage"><ExploreBoard :selection="activeHardware" :pin="hardwarePin" :running="running" :flow="flow" :color="output?.color" :led-level="output?.value" @select="selectHardware" /></div>
      <div v-if="activeHardware!=='EX1' && lesson.params.length" class="inline-parameters"><label v-for="p in lesson.params.slice(0,2)" :key="p.key"><span>{{en?p.en:p.zh}} <small class="parameter-scope">{{firmwareParameters[selected]?.[p.key]?tr('固件参数','Firmware parameter'):tr('模拟输入','Simulation input')}}</small><output>{{values[p.key]}} {{p.unit}}</output></span><input v-model.number="values[p.key]" type="range" :min="p.min" :max="p.max" :step="p.step" :aria-label="(en?p.en:p.zh)+tr(' · 主面板',' · Main panel')"/></label><button @click="inspector=true" :aria-label="tr('全部课程参数', 'All lesson parameters')"><SlidersHorizontal/></button><button @click="hardwareSelection=lessonHardware[selected]" class="view-lesson-pins">{{tr('本课引脚','Lesson pins')}}<ArrowUpRight/></button></div>
      <section
        v-if="output"
        class="instrument"
        :aria-label="tr('模拟输出', 'Simulation output')"
        :data-running="running"
      >
        <header>
          <span class="live-dot" :class="{ running }"></span>{{ output.label
          }}<small>{{ tr("教学模拟", "SIMULATION") }}</small>
        </header>
        <div class="instrument-value">
          <span :style="{ color: selected === 1 ? output.color : undefined }">{{
            displayValue
          }}</span
          ><small>{{ output.unit }}</small
          ><component
            :is="icons[selected]"
            :style="{
              transform: `rotate(${output.angle}deg)`,
              color: selected === 1 ? output.color : undefined,
            }"
          />
        </div>
        <p>{{ output.secondary }}</p>
        <canvas
          ref="graph"
          width="480"
          height="90"
          :aria-label="tr('模拟输出趋势图', 'Simulation output history')"
        ></canvas>
      </section>
    </main>
    <aside class="experiment-aside">
      <PartInspector :id="activeHardware" :running="running" :output="output" :flow="flow" @pin="hardwarePin=$event" />
      <section class="experiment-code">
        <header>
          <h2>{{ tr("动手试试", "Try it yourself") }}</h2>
          <div>
            <button @click="deviceOpen=true" :aria-label="tr('下载到开发板', 'Download to controller')" :title="tr('编译并下载到真实开发板', 'Compile and download to the controller')"><Download/></button>
            <button
              @click="expanded = !expanded"
              :aria-label="
                expanded
                  ? tr('收起代码', 'Collapse code')
                  : tr('展开代码', 'Expand code')
              "
            >
              <Minimize2 v-if="expanded" /><Maximize2 v-else /></button
            ><button @click="copy" :aria-label="tr('复制代码', 'Copy code')">
              <Check v-if="copied" /><Copy v-else /></button
            ><button
              class="run-button"
              :class="{ running }"
              @click="run"
              :aria-label="
                running
                  ? tr('停止程序', 'Stop program')
                  : tr('网页运行', 'Run simulation')
              "
            >
              <Square v-if="running" /><Play v-else /><span>{{ running ? tr("停止", "Stop") : tr("网页运行", "Simulate") }}</span>
            </button>
          </div>
        </header>
        <div class="code-mode">
          <button :aria-pressed="!showSource" @click="showSource = false">
            {{ tr("关键逻辑", "Key logic") }}</button
          ><button v-if="activeHardware!=='EX1'" :aria-pressed="showSource" @click="showSource = true">
            {{ tr("完整源码", "Full source") }}</button
          ><button
            @click="inspector = !inspector"
            :aria-label="tr('调整参数', 'Adjust parameters')"
          >
            <SlidersHorizontal />
          </button>
        </div>
        <div
          v-if="!showSource"
          ref="codeLines"
          class="execution-code"
          tabindex="0"
          :aria-label="tr('教学代码节选', 'Teaching code excerpt')"
        >
          <div
            v-for="(line, i) in code"
            :key="i"
            :class="{
              executing: running && output?.line === i,
              comment: line.startsWith('//'),
            }"
            :data-active="running && output?.line === i"
          >
            <span class="line-number">{{ i + 1 }}</span
            ><code
              ><button
                v-if="/=\s*[-\d.]+;/.test(line)"
                class="code-parameter"
                @click="inspector = true"
                :aria-label="tr('修改参数：', 'Adjust parameter: ') + line"
              >
                {{ line }}</button
              ><template v-else>{{ line }}</template></code
            >
          </div>
        </div>
        <template v-else
          ><div class="full-source-controls">
            <select
              v-model="sourceIndex"
              :aria-label="tr('源文件', 'Source file')"
            >
              <option v-for="(f, i) in files" :key="f.name" :value="i">
                {{ f.name }}
              </option></select
            ><button
              @click="download"
              :disabled="!source"
              :aria-label="tr('下载完整源码', 'Download full source')"
            >
              <Download />
            </button>
          </div>
          <div v-if="source" class="full-source"><pre v-if="Object.keys(firmwareParameters[selected]||{}).length" class="build-parameters">{{parameterHeader}}</pre><div v-html="source.html"></div></div>
          <div v-else class="source-message">
            {{
              files.length
                ? sourceError
                  ? tr("加载失败", "Load failed")
                  : tr("正在加载", "Loading")
                : tr(
                    "请从 LED 课程开始查看例程",
                    "Start with the LED lesson for source code",
                  )
            }}<button v-if="sourceError" @click="loadSource">
              {{ tr("重试", "Retry") }}
            </button>
          </div></template
        >
        <div v-if="logs.length" class="simulation-console">
          <header>
            <span class="live-dot" :class="{ running }"></span
            >{{ tr("模拟日志", "Simulation log")
            }}<span>{{
              running ? tr("运行中", "Running") : tr("已停止", "Stopped")
            }}</span>
          </header>
          <div>
            <p v-for="(log, i) in logs.slice(-4)" :key="i">
              <time>{{ log.time }}</time
              >{{ log.text }}
            </p>
          </div>
        </div>
        <footer>
          {{
            tr(
              "课程行为模型 · 不执行固件",
              "Guided model · no firmware execution",
            )
          }}<button
            v-if="selected === 2"
            @click="enableAudio"
            :aria-label="tr('切换声音', 'Toggle sound')"
          >
            <Volume2 v-if="audioEnabled" /><VolumeX v-else />
          </button>
        </footer>
      </section>
    </aside>
    <section
      class="experiment-courses"
      :aria-label="tr('课程选择', 'Choose a lesson')"
    >
      <header>
        <h2>{{ tr("课程", "Lessons") }}</h2>
        <span>{{ selected + 1 }} / {{ lessons.length }}</span>
        <div>
          <button
            :disabled="selected === 0"
            @click="choose(selected - 1)"
            :aria-label="tr('上一课', 'Previous lesson')"
          >
            <ChevronLeft /></button
          ><button
            :disabled="selected === 13"
            @click="choose(selected + 1)"
            :aria-label="tr('下一课', 'Next lesson')"
          >
            <ChevronRight />
          </button>
        </div>
      </header>
      <div ref="courseRow" class="experiment-course-row">
        <button
          v-for="l in lessons"
          :key="l.id"
          :class="{ selected: l.id === selected }"
          :aria-pressed="l.id === selected"
          @click="choose(l.id)"
        >
          <span class="course-art"><component :is="icons[l.id]" /></span
          ><span
            ><small>{{ String(l.id).padStart(2, "0") }}</small
            ><strong>{{ en ? l.en : l.zh }}</strong></span
          ><Check
            v-if="progress.includes(l.id)"
            class="course-complete"
          /><ChevronRight v-else />
        </button>
      </div>
    </section>
    <section
      v-if="inspector"
      class="component-inspector"
      role="dialog"
      :aria-label="tr('元件检查器', 'Component inspector')"
    >
      <header>
        <component :is="icons[selected]" /><strong>{{ lesson.part }}</strong
        ><button
          @click="inspector = false"
          :aria-label="tr('关闭参数', 'Close parameters')"
        >
          <X />
        </button>
      </header>
      <p>{{ en ? lesson.descriptionEn : lesson.description }}</p>
      <label v-for="p in lesson.params" :key="p.key"
        ><span
          >{{ en ? p.en : p.zh
          }}<output>{{ values[p.key] }} {{ p.unit }}</output></span
        ><input
          v-model.number="values[p.key]"
          type="range"
          :min="p.min"
          :max="p.max"
          :step="p.step"
          :aria-label="en ? p.en : p.zh"
      /></label>
      <p v-if="!lesson.params.length">
        {{
          tr(
            "选择其他课程，开始调整外设。",
            "Choose another lesson to adjust a peripheral.",
          )
        }}
      </p>
      <div v-if="selected === 7" class="nvs-actions">
        <button @click="saveNvs"><Save />{{ tr("保存参数", "Save") }}</button
        ><button @click="restartNvs">
          <RotateCcw />{{ tr("模拟重启", "Restart") }}
        </button>
      </div>
      <button v-if="selected === 2" class="sound-toggle" @click="enableAudio">
        <Volume2 />{{
          audioEnabled ? tr("关闭声音", "Mute") : tr("开启声音", "Enable sound")
        }}</button
      ><button class="inspector-run" @click="run">
        <Square v-if="running" /><Play v-else />{{
          running
            ? tr("停止模拟", "Stop simulation")
            : tr("运行模拟", "Run simulation")
        }}
      </button>
    </section>
    <section
      v-if="settings"
      class="experiment-settings"
      role="dialog"
      :aria-label="tr('设置', 'Settings')"
    >
      <header>
        <h2>{{ tr("设置", "Settings") }}</h2>
        <button
          @click="settings = false"
          :aria-label="tr('关闭设置', 'Close settings')"
        >
          <X />
        </button>
      </header>
      <label
        ><input type="checkbox" v-model="flow" />{{
          tr("显示信号路径", "Show signal path")
        }}</label
      >
      <p>
        {{
          tr(
            "空格运行或停止；左右方向键切课；Esc 关闭面板。",
            "Space runs/stops; arrows change lessons; Esc closes panels.",
          )
        }}
      </p>
      <p>
        {{
          tr(
            "模拟数据不来自真实开发板，复杂电机与传感器行为作教学简化。",
            "Simulation data is independent of hardware. Motor and sensor models are simplified for teaching.",
          )
        }}
      </p>
    </section>
  </div>
</template>
