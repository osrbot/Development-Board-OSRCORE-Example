<script setup lang="ts">
import { ref, computed } from 'vue'
import FlashTool from './FlashTool.vue'

const props = defineProps<{ locale?: string }>()

const isEn = computed(() => props.locale === 'en')

const examples = [
  { id: '01_blink_led',    zh: 'Ch 01 — WS2812B RGB LED',          en: 'Ch 01 — WS2812B RGB LED' },
  { id: '02_buzzer',       zh: 'Ch 02 — 无源蜂鸣器',               en: 'Ch 02 — Passive Buzzer' },
  { id: '03_pwm_servo',    zh: 'Ch 03 — PWM 舵机与电调',           en: 'Ch 03 — PWM Servo & ESC' },
  { id: '04_sbus_rc',      zh: 'Ch 04 — SBUS 遥控接收',            en: 'Ch 04 — SBUS RC Receiver' },
  { id: '05_imu_i2c',      zh: 'Ch 05 — QMI8658 IMU',              en: 'Ch 05 — QMI8658 IMU' },
  { id: '06_encoder',      zh: 'Ch 06 — 正交编码器',               en: 'Ch 06 — Quadrature Encoder' },
  { id: '07_nvs',          zh: 'Ch 07 — NVS 参数持久化',           en: 'Ch 07 — NVS Persistence' },
  { id: '08_freertos',     zh: 'Ch 08 — FreeRTOS 多任务',          en: 'Ch 08 — FreeRTOS Tasks' },
  { id: '09_pid_motor',    zh: 'Ch 09 — PID 电机速度闭环',         en: 'Ch 09 — PID Speed Loop' },
  { id: '10_ahrs',         zh: 'Ch 10 — Madgwick AHRS',            en: 'Ch 10 — Madgwick AHRS' },
  { id: '11_mag_compass',  zh: 'Ch 11 — QMC6309 磁力计',           en: 'Ch 11 — QMC6309 Magnetometer' },
  { id: '12_full_example', zh: 'Ch 12 — 完整机器人示例',           en: 'Ch 12 — Full Robot Example' },
]

const selected = ref(examples[0].id)

const selectedLabel = computed(() => {
  const ex = examples.find(e => e.id === selected.value)!
  return isEn.value ? ex.en : ex.zh
})
</script>

<template>
  <ClientOnly>
    <div class="flash-hub">
      <div class="flash-hub-select-row">
        <label class="flash-hub-label" for="example-select">
          {{ isEn ? 'Select example:' : '选择示例：' }}
        </label>
        <select id="example-select" v-model="selected" class="flash-hub-select">
          <option v-for="ex in examples" :key="ex.id" :value="ex.id">
            {{ isEn ? ex.en : ex.zh }}
          </option>
        </select>
      </div>
      <FlashTool :key="selected" :example="selected" :label="selectedLabel" />
    </div>
  </ClientOnly>
</template>

<style scoped>
.flash-hub { margin: 20px 0; }

.flash-hub-select-row {
  display: flex;
  align-items: center;
  gap: 12px;
  margin-bottom: 16px;
}

.flash-hub-label {
  font-size: 14px;
  font-weight: 500;
  color: var(--vp-c-text-2);
  white-space: nowrap;
}

.flash-hub-select {
  flex: 1;
  max-width: 360px;
  padding: 7px 12px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
  font-size: 14px;
  cursor: pointer;
  outline: none;
}
.flash-hub-select:focus {
  border-color: var(--vp-c-brand-1);
}
</style>
