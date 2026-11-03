<script setup lang="ts">
import { computed, ref, onMounted, onUnmounted, watch, nextTick } from "vue";
import { withBase } from "vitepress";
import {
  ArrowUpRight,
  ArrowRight,
  ChevronLeft,
  ChevronRight,
  Check,
  Copy,
  Maximize2,
  Minimize2,
  BookOpen,
  Code2,
  Download,
  MapPin,
  FileText,
  Layers,
  Box,
  Camera,
  Lightbulb,
  Volume2,
  SlidersHorizontal,
  Radio,
  Compass,
  Gauge,
  HardDrive,
  Cpu,
  Activity,
  Navigation,
  Magnet,
  Bot,
  PlugZap,
  Plus,
} from "lucide-vue-next";
import BoardViewer from "./BoardViewer.vue";
import courses from "./courses.json";
const props = defineProps<{ locale?: string }>();
const en = computed(() => props.locale === "en");
const tr = (zh: string, enText: string) => (en.value ? enText : zh);
const selected = ref(0),
  tab = ref("overview"),
  view = ref("3d"),
  photo = ref("angle"),
  file = ref(0),
  copyState = ref(""),
  completed = ref<string[]>([]),
  codeExpanded = ref(false);
const courseRow = ref<HTMLElement>(),
  infoBody = ref<HTMLElement>();
const lesson = computed(() => courses[selected.value]);
const text = computed(() => lesson.value[en.value ? "en" : "zh"]);
const prefix = computed(() => (en.value ? "/en" : ""));
const courseIcons = [
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
];
const shortTitles = [
  ["认识开发板", "Meet OSRCORE"],
  ["点亮 RGB 灯", "Light up an LED"],
  ["让蜂鸣器发声", "Make some sound"],
  ["控制舵机与电调", "Servo & ESC"],
  ["接收遥控信号", "Read RC signals"],
  ["感知运动", "Sense motion"],
  ["测量转速", "Measure speed"],
  ["保存参数", "Save settings"],
  ["运行多个任务", "Run multiple tasks"],
  ["闭环控制电机", "Control motor speed"],
  ["解算姿态", "Estimate attitude"],
  ["找到方向", "Find your heading"],
  ["组合成机器人", "Build a robot"],
];
const summary = computed(() =>
  selected.value === 0
    ? tr(
        "从 ESP-IDF 环境和硬件接口开始，把第一段程序运行在 OSRCORE 上。",
        "Start with ESP-IDF and the board interfaces, then run your first program on OSRCORE.",
      )
    : text.value.intro.replace(/\*\*|`/g, "").split("\n\n")[0],
);
const pinGroups = [
  {
    ids: [0, 7, 8, 12],
    name: "ESP32-S3",
    pin: "N16R8 · USB-C",
    x: 52,
    y: 45,
    zh: "主控与 USB 调试接口。先完成工具链配置，再运行示例。",
    en: "Main controller and USB debugging. Set up ESP-IDF before running the examples.",
  },
  {
    ids: [1],
    name: "RGB LED",
    pin: "GPIO 45 · RMT",
    x: 93,
    y: 88,
    zh: "板载 WS2812 RGB 灯，无需外部接线。红、绿、蓝、熄灭，每 500 ms 切换。",
    en: "Onboard WS2812. No external wiring. Red, green, blue, off; each lasts 500 ms.",
  },
  {
    ids: [2],
    name: "Buzzer",
    pin: "GPIO 42 · PWM",
    x: 50,
    y: 20,
    zh: "板载无源蜂鸣器，无需外部接线。PWM 控制音调，示例依次播放音阶。",
    en: "Onboard passive buzzer. No external wiring. PWM changes pitch; the example plays a scale.",
  },
  {
    ids: [3, 9],
    name: "PWM / ESC",
    pin: "GPIO 1 / 2",
    x: 89,
    y: 76,
    zh: "EX2 的 ESC / SER 信号用于油门与转向，连接时核对 IO / V / G 标识。电机实验先架空车轮。",
    en: "ESC / SER signals on EX2 control throttle and steering. Check IO / V / G and lift the wheels before motor experiments.",
  },
  {
    ids: [4],
    name: "SBUS",
    pin: "GPIO 44 · UART RX",
    x: 89,
    y: 59,
    zh: "按完整教程连接接收机信号与公共地，并核对接收机供电要求。",
    en: "Follow the full guide for receiver signal and common ground; verify receiver power requirements.",
  },
  {
    ids: [5, 10, 11],
    name: "IMU / Magnetometer",
    pin: "SDA 10 · SCL 11",
    x: 11,
    y: 44,
    zh: "板载传感器通过 I²C 连接，无需额外接线。姿态与罗盘实验需要按教程校准。",
    en: "Onboard sensors share I²C; no extra wiring. Follow the guide for attitude and compass calibration.",
  },
  {
    ids: [6],
    name: "Encoder",
    pin: "EA 3 · EB 9",
    x: 89,
    y: 66,
    zh: "按 EX2 的 EA / EB 标识连接编码器，确认电源及公共地。每转脉冲数和减速比必须使用实际电机参数。",
    en: "Connect the encoder using EA / EB on EX2, with correct power and common ground. Use the actual encoder resolution and gear ratio.",
  },
];
const pin = computed(
  () => pinGroups.find((p) => p.ids.includes(selected.value)) || pinGroups[0],
);
const activeHotspot = ref("");
const shownPin = computed(
  () => pinGroups.find((p) => p.name === activeHotspot.value) || pin.value,
);
const loadedCode = ref<{
    code: string;
    html: string;
    snippet: string;
    excerpt: string;
  } | null>(null),
  codeError = ref("");
const code = computed(() => loadedCode.value?.code || "");
let request = 0,
  mounted = false;
async function loadCode() {
  const current = ++request;
  loadedCode.value = null;
  codeError.value = "";
  const source = lesson.value.files[file.value];
  if (!source) return;
  try {
    const response = await fetch(withBase(source.url));
    if (!response.ok) throw Error(String(response.status));
    const data = await response.json();
    if (current === request) loadedCode.value = data;
  } catch {
    if (current === request)
      codeError.value = tr(
        "代码暂未加载，请重试。",
        "Could not load the code. Try again.",
      );
  }
}
watch(
  () => [selected.value, file.value],
  () => {
    if (mounted) loadCode();
  },
);
function showSelectedCourse() {
  nextTick(() => {
    const row = courseRow.value,
      button = row?.querySelector<HTMLElement>('[aria-pressed="true"]');
    if (row && button) {
      const left = button.offsetLeft - row.offsetLeft;
      if (left < row.scrollLeft) row.scrollTo({ left });
      else if (left + button.offsetWidth > row.scrollLeft + row.clientWidth)
        row.scrollTo({ left: left + button.offsetWidth - row.clientWidth });
    }
  });
}
function choose(i: number) {
  if (i < 0 || i >= courses.length) return;
  selected.value = i;
  file.value = 0;
  activeHotspot.value = "";
  copyState.value = "";
  if (infoBody.value) infoBody.value.scrollTop = 0;
  if (typeof window !== "undefined")
    history.replaceState(null, "", `#lesson-${courses[i].id}`);
  showSelectedCourse();
}
function readHash() {
  const m = location.hash.match(/^#lesson-(\d\d)$/);
  if (m) {
    const i = courses.findIndex((c) => c.id === m[1]);
    if (i >= 0) {
      selected.value = i;
      file.value = 0;
      activeHotspot.value = "";
    }
  }
  showSelectedCourse();
}
onMounted(() => {
  mounted = true;
  readHash();
  loadCode();
  window.addEventListener("hashchange", readHash);
  try {
    const v = JSON.parse(localStorage.getItem("osrcore-lessons") || "[]");
    if (Array.isArray(v))
      completed.value = v.filter((x) => typeof x === "string");
  } catch {}
});
onUnmounted(() => {
  request++;
  window.removeEventListener("hashchange", readHash);
});
function complete() {
  const id = lesson.value.id;
  completed.value = completed.value.includes(id)
    ? completed.value.filter((x) => x !== id)
    : [...completed.value, id];
  try {
    localStorage.setItem("osrcore-lessons", JSON.stringify(completed.value));
  } catch {}
}
async function copy() {
  try {
    if (navigator.clipboard && window.isSecureContext)
      await navigator.clipboard.writeText(code.value);
    else {
      const el = document.createElement("textarea");
      el.value = code.value;
      el.style.position = "fixed";
      el.style.opacity = "0";
      document.body.appendChild(el);
      el.select();
      const ok = document.execCommand("copy");
      el.remove();
      if (!ok) throw Error();
    }
    copyState.value = tr("已复制", "Copied");
  } catch {
    copyState.value = tr("请选中代码复制", "Select code to copy");
  }
}
function scrollCourses(direction: number) {
  courseRow.value?.scrollBy({
    left: direction * courseRow.value.clientWidth * 0.7,
    behavior: matchMedia("(prefers-reduced-motion: reduce)").matches
      ? "auto"
      : "smooth",
  });
}
</script>
<template>
  <main id="learning-main" class="learning-lab" tabindex="-1">
    <header class="lab-topbar">
      <div class="lab-title">
        <span class="eyebrow">OSRCORE</span>
        <h1>{{ tr("开发板实验室", "Board laboratory") }}</h1>
        <span class="lab-subtitle">{{
          tr("探索 · 理解 · 动手", "Explore · Understand · Build")
        }}</span>
      </div>
      <div class="learning-progress">
        <BookOpen />
        <div>
          <span
            >{{ tr("学习进度", "Your progress") }}
            <b>{{ completed.length }} / {{ courses.length }}</b></span
          ><progress
            :max="courses.length"
            :value="completed.length"
            :aria-label="tr('学习进度', 'Learning progress')"
          ></progress>
        </div>
      </div>
    </header>
    <div class="lab-workspace">
      <section
        class="board-stage"
        :aria-label="tr('开发板探索', 'Explore the board')"
      >
        <div class="stage-topline">
          <span class="stage-kicker">ESP32-S3 <span>·</span> OSRCORE</span>
          <div class="view-tabs" :aria-label="tr('板卡视图', 'Board view')">
            <button
              v-for="v in [
                { id: '3d', zh: '三维', en: '3D', icon: Box },
                { id: 'photo', zh: '实拍', en: 'Photos', icon: Camera },
                { id: 'front', zh: '接口图', en: 'Pin map', icon: Layers },
                
              ]"
              :key="v.id"
              :aria-pressed="view === v.id"
              @click="view = v.id"
            >
              <component :is="v.icon" />{{ en ? v.en : v.zh }}
            </button>
          </div>
        </div>
        <div class="board-display">
          <BoardViewer
            v-show="view === '3d'"
            :active="view === '3d'"
            :en="en"
            :markers="pinGroups"
            :selected="shownPin.name"
            @select="activeHotspot = $event"
          />
          <div v-if="view === 'photo'" class="photo-stage">
            <img :src="withBase('/hardware/photo-' + photo + '.jpg')" :alt="tr('OSRCORE 开发板实拍照片', 'Original OSRCORE board photograph')" />
            <div class="photo-choices" :aria-label="tr('实拍角度', 'Photo angle')">
              <button v-for="p in [{id:'angle',zh:'整体',en:'Perspective'}, {id:'front',zh:'正面',en:'Front'}, {id:'back',zh:'背面',en:'Back'}]" :key="p.id" :aria-pressed="photo === p.id" @click="photo = p.id">{{ en ? p.en : p.zh }}</button>
            </div>
          </div>
          <div v-if="view === 'front'" class="board-image-stage">
            <div class="board-image-wrap">
              <img
                :src="
                  withBase(
                    '/hardware/' +
                      (view === 'back' ? 'back' : 'front') +
                      '.png',
                  )
                "
                :alt="tr('OSRCORE 板卡布局', 'OSRCORE board layout')"
              /><template v-if="view === 'front'"
                ><button
                  v-for="p in pinGroups"
                  :key="p.name"
                  class="hotspot"
                  :class="{ active: shownPin.name === p.name }"
                  :style="{ left: p.x + '%', top: p.y + '%' }"
                  :aria-label="p.name + ' · ' + p.pin"
                  :aria-pressed="shownPin.name === p.name"
                  @click="activeHotspot = p.name"
                >
                  <Plus /></button
              ></template>
            </div>
          </div>
        </div>
        <div class="connection-strip" aria-live="polite">
          <div class="connection-symbol">
            <component :is="courseIcons[selected]" />
          </div>
          <div class="connection-copy">
            <div>
              <strong>{{ shownPin.name }}</strong
              ><code>{{ shownPin.pin }}</code>
            </div>
            <p>{{ en ? shownPin.en : shownPin.zh }}</p>
          </div>
          <button
            class="icon-button"
            :aria-label="tr('在正面图上定位', 'Locate in front view')"
            @click="view = 'front'"
          >
            <MapPin />
          </button>
        </div>
      </section>
      <aside
        class="lab-aside"
        :class="{ 'code-expanded': codeExpanded }"
        :aria-label="tr('课程与代码', 'Lesson and code')"
      >
        <section class="lesson-info">
          <div class="lesson-tabs">
            <button
              v-for="v in [
                { id: 'overview', zh: '概览', en: 'Overview' },
                { id: 'connection', zh: '接线', en: 'Wiring' },
                { id: 'resources', zh: '资料', en: 'Files' },
              ]"
              :key="v.id"
              :aria-pressed="tab === v.id"
              @click="tab = v.id"
            >
              {{ en ? v.en : v.zh }}</button
            ><button
              class="completion icon-button"
              :class="{ 'is-done': completed.includes(lesson.id) }"
              :aria-label="
                completed.includes(lesson.id)
                  ? tr('取消完成标记', 'Mark incomplete')
                  : tr('标记已完成', 'Mark complete')
              "
              :aria-pressed="completed.includes(lesson.id)"
              @click="complete"
            >
              <Check />
            </button>
          </div>
          <div
            ref="infoBody"
            class="lesson-info-body"
            tabindex="0"
            :aria-label="tr('课程说明', 'Lesson details')"
          >
            <div class="lesson-number">
              {{ tr("第", "LESSON") }} {{ lesson.id }} {{ tr("课", "")
              }}<span>{{
                selected < 4
                  ? tr("基础入门", "FOUNDATIONS")
                  : selected < 9
                    ? tr("外设与系统", "PERIPHERALS")
                    : tr("机器人控制", "ROBOTICS")
              }}</span>
            </div>
            <h2>{{ shortTitles[selected][en ? 1 : 0] }}</h2>
            <template v-if="tab === 'overview'"
              ><p class="lesson-description">{{ summary }}</p>
              <ul class="learning-points">
                <li v-for="point in text.points.slice(0, 3)" :key="point">
                  <Check /><span>{{ point.replace(/`/g, "") }}</span>
                </li>
              </ul></template
            >
            <template v-else-if="tab === 'connection'"
              ><div class="pin-badge">
                <MapPin /><code>{{ shownPin.pin }}</code>
              </div>
              <p class="lesson-description">
                {{ en ? shownPin.en : shownPin.zh }}
              </p>
              <button class="text-button" @click="view = 'front'">
                {{ tr("在板上查看位置", "Locate on the board")
                }}<ArrowRight /></button
              ><a
                class="resource-link"
                :href="
                  withBase(
                    '/hardware/interfaces-' + (en ? 'en' : 'zh') + '.png',
                  )
                "
                target="_blank"
                ><FileText />{{ tr("打开完整接口图", "Open interface diagram")
                }}<ArrowUpRight /></a
            ></template>
            <div v-else class="resources">
              <a
                v-for="r in [
                  {
                    name: tr('产品手册', 'Product manual'),
                    path: 'manual-' + (en ? 'en' : 'zh') + '.pdf',
                    type: 'PDF',
                  },
                  {
                    name: tr('原理图', 'Schematic'),
                    path: 'schematic-' + (en ? 'en' : 'zh') + '.pdf',
                    type: 'PDF',
                  },
                  {
                    name: tr('接口图', 'Interface diagram'),
                    path: 'interfaces-' + (en ? 'en' : 'zh') + '.png',
                    type: 'PNG',
                  },
                  {
                    name: tr('三维模型', '3D model'),
                    path: 'osrcore.glb',
                    type: 'GLB',
                  },
                ]"
                :key="r.path"
                :href="withBase('/hardware/' + r.path)"
                target="_blank"
                ><FileText /><span>{{ r.name }}</span
                ><small>{{ r.type }}</small
                ><ArrowUpRight
              /></a>
            </div>
          </div>
          <a
            class="lesson-full-link"
            :href="withBase(prefix + '/chapter-' + lesson.id)"
            ><BookOpen />{{ tr("阅读完整教程", "Read the full lesson")
            }}<ArrowUpRight
          /></a>
        </section>
        <section class="code-dock" :aria-label="tr('示例代码', 'Example code')">
          <header class="code-header">
            <h3><Code2 />{{ tr("动手试试", "Try it yourself") }}</h3>
            <div>
              <button
                class="icon-button"
                :aria-label="
                  codeExpanded
                    ? tr('收起代码', 'Collapse code')
                    : tr('展开代码', 'Expand code')
                "
                :aria-pressed="codeExpanded"
                @click="codeExpanded = !codeExpanded"
              >
                <Minimize2 v-if="codeExpanded" /><Maximize2 v-else /></button
              ><button
                class="icon-button"
                :disabled="!loadedCode"
                :aria-label="
                  copyState || tr('复制完整代码', 'Copy full source')
                "
                @click="copy"
              >
                <Check v-if="copyState" /><Copy v-else /></button
              ><a class="flash-button" :href="withBase(prefix + '/updater')"
                ><PlugZap />{{ tr("烧录", "Flash") }}</a
              >
            </div>
          </header>
          <div v-if="lesson.files.length" class="source-selector">
            <select
              v-model="file"
              :aria-label="tr('选择源文件', 'Choose source file')"
            >
              <option v-for="(f, i) in lesson.files" :key="f.name" :value="i">
                {{ f.name }}
              </option></select
            ><span>{{
              codeExpanded
                ? tr("完整文件", "Full source")
                : loadedCode?.excerpt === "app_main"
                  ? "app_main"
                  : tr("代码节选", "Excerpt")
            }}</span>
          </div>
          <div
            class="code-scroll"
            tabindex="0"
            :aria-label="tr('源代码内容', 'Source code content')"
          >
            <div
              v-if="loadedCode"
              class="highlighted-code"
              v-html="codeExpanded ? loadedCode.html : loadedCode.snippet"
            ></div>
            <div v-else-if="codeError" class="code-notice">
              <p>{{ codeError }}</p>
              <button @click="loadCode">{{ tr("重试", "Retry") }}</button>
            </div>
            <p
              v-else-if="lesson.files.length"
              class="code-notice"
              role="status"
            >
              {{ tr("正在加载代码…", "Loading source…") }}
            </p>
            <pre
              v-else
              class="setup-code"
            ><code># ESP-IDF
cd 01_blink_led
idf.py set-target esp32s3
idf.py build
idf.py flash monitor</code></pre>
          </div>
          <footer class="code-footer">
            <span class="status-dot"></span
            >{{
              tr(
                "ESP-IDF · 在真实开发板上运行",
                "ESP-IDF · Runs on your real board",
              )
            }}<span v-if="copyState" role="status">{{ copyState }}</span>
          </footer>
        </section>
      </aside>
    </div>
    <section class="course-section" :aria-label="tr('全部课程', 'All lessons')">
      <header class="course-heading">
        <h2>
          {{ tr("课程", "Lessons")
          }}<span>{{ Number(lesson.id) + 1 }} / {{ courses.length }}</span>
        </h2>
        <div class="course-navigation">
          <button
            class="icon-button"
            :disabled="selected === 0"
            :aria-label="tr('上一课', 'Previous lesson')"
            @click="choose(selected - 1)"
          >
            <ChevronLeft /></button
          ><button
            class="icon-button"
            :disabled="selected === courses.length - 1"
            :aria-label="tr('下一课', 'Next lesson')"
            @click="choose(selected + 1)"
          >
            <ChevronRight /></button
          ><a :href="withBase(prefix + '/chapters')"
            >{{ tr("全部文档", "All docs") }}<ArrowUpRight
          /></a>
        </div>
      </header>
      <div class="course-ribbon">
        <button
          class="ribbon-arrow icon-button"
          :aria-label="tr('向前浏览课程', 'Scroll lessons back')"
          @click="scrollCourses(-1)"
        >
          <ChevronLeft />
        </button>
        <div ref="courseRow" class="course-row">
          <button
            v-for="(c, i) in courses"
            :key="c.id"
            :class="{
              selected: selected === i,
              done: completed.includes(c.id),
            }"
            :aria-label="c.id + ' ' + shortTitles[i][en ? 1 : 0]"
            :aria-pressed="selected === i"
            @click="choose(i)"
          >
            <span class="course-icon"
              ><Check v-if="completed.includes(c.id)" /><component
                v-else
                :is="courseIcons[i]" /></span
            ><span class="course-label"
              ><small
                >{{ tr("第", "LESSON") }} {{ c.id }} {{ tr("课", "") }}</small
              ><strong>{{ shortTitles[i][en ? 1 : 0] }}</strong></span
            ><ChevronRight />
          </button>
        </div>
        <button
          class="ribbon-arrow icon-button"
          :aria-label="tr('向后浏览课程', 'Scroll lessons forward')"
          @click="scrollCourses(1)"
        >
          <ChevronRight />
        </button>
      </div>
    </section>
  </main>
</template>
