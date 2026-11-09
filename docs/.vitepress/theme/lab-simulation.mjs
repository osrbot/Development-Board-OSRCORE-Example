// Original, guided teaching models. These do not execute or compile ESP-IDF binaries.
const p = (key, zh, en, min, max, step, value, unit = "") => ({
  key,
  zh,
  en,
  min,
  max,
  step,
  value,
  unit,
});
const d = (id, zh, en, description, descriptionEn, part, pin, params) => ({
  id,
  zh,
  en,
  description,
  descriptionEn,
  part,
  pin,
  params,
});
export const lessons = [
  d(
    0,
    "认识开发板",
    "Meet OSRCORE",
    "从供电、主控到输入输出。点选板上的标记，查看对应接口；运行一次启动流程。",
    "Explore the processor, power and I/O. Select a component and run the startup sequence.",
    "ESP32-S3",
    "N16R8 · USB-C",
    [],
  ),
  d(
    1,
    "点亮 RGB 灯",
    "Light up an LED",
    "让板载 RGB 灯依次变成红、绿、蓝。改变间隔和亮度，观察灯光、代码与输出同步变化。",
    "Cycle the onboard LED through red, green and blue. Change brightness and timing, then compare code and output.",
    "RGB LED",
    "GPIO 45 · RMT",
    [
      p("interval", "切换间隔", "Interval", 100, 2000, 50, 500, "ms"),
      p("brightness", "亮度", "Brightness", 0, 255, 1, 32),
    ],
  ),
  d(
    2,
    "让蜂鸣器发声",
    "Make some sound",
    "改变 PWM 频率就是改变音高。开启声音后，点击运行即可听到所选音调。",
    "PWM frequency controls pitch. Enable sound and run to hear the selected tone.",
    "Buzzer",
    "GPIO 42 · PWM",
    [
      p("frequency", "音调频率", "Frequency", 220, 1760, 10, 440, "Hz"),
      p("duration", "发声时间", "Duration", 100, 1000, 50, 300, "ms"),
    ],
  ),
  d(
    3,
    "控制舵机与电调",
    "Servo & ESC",
    "50 Hz PWM 用脉宽表达位置。拖动脉宽，观察舵机指针和占空比；1500 µs 为中立。",
    "50 Hz PWM encodes position as pulse width. Move the pulse slider and observe position and duty; 1500 µs is neutral.",
    "PWM / ESC",
    "GPIO 1 / 2",
    [p("pulse", "脉宽", "Pulse width", 1000, 2000, 10, 1500, "µs")],
  ),
  d(
    4,
    "接收遥控信号",
    "Read RC signals",
    "模拟 SBUS 通道输入，查看解码值。打开失控保护，观察输出回到中立。",
    "Simulate an SBUS channel and inspect its decoded value. Trigger failsafe to return the output to neutral.",
    "SBUS",
    "GPIO 44 · UART RX",
    [
      p("channel", "通道值", "Channel", 240, 1810, 1, 1024),
      p("failsafe", "失控保护", "Failsafe", 0, 1, 1, 0),
    ],
  ),
  d(
    5,
    "感知运动",
    "Sense motion",
    "倾斜板卡时，重力会投影到不同轴。改变倾角，观察理想加速度读数。",
    "Tilting the board distributes gravity across its axes. Change the tilt to observe ideal accelerometer readings.",
    "IMU",
    "SDA 10 · SCL 11",
    [p("angle", "倾角", "Tilt", -80, 80, 1, 0, "°")],
  ),
  d(
    6,
    "测量转速",
    "Measure speed",
    "编码器脉冲可换算为转速。改变每秒脉冲数或每转计数，比较计数和 RPM。",
    "Convert encoder counts to speed. Change counts per second or per revolution and compare the RPM.",
    "Encoder",
    "EA 3 · EB 9",
    [
      p("pps", "每秒计数", "Counts / second", 0, 20000, 100, 5400),
      p("ppr", "输出轴每转计数", "Counts / revolution", 100, 20000, 100, 10800),
    ],
  ),
  d(
    7,
    "保存参数",
    "Save settings",
    "把参数写入模拟 NVS，再模拟重启。保存值只存于当前浏览器，与实体开发板无关。",
    "Write a parameter to simulated NVS and restart. This value is stored in your browser, independently of any physical board.",
    "ESP32-S3",
    "NVS · Flash",
    [p("kp", "Kp 参数", "Kp", 0, 20, 0.1, 2.5)],
  ),
  d(
    8,
    "运行多个任务",
    "Run multiple tasks",
    "两个周期任务交替产生输出。改变周期，观察任务时间轴；这里演示调度概念，不模拟双核抢占。",
    "Two periodic tasks produce interleaved output. Change their periods to explore scheduling, without emulating dual-core preemption.",
    "ESP32-S3",
    "FreeRTOS",
    [
      p("taskA", "任务 A 周期", "Task A period", 100, 1500, 100, 500, "ms"),
      p("taskB", "任务 B 周期", "Task B period", 100, 1500, 100, 1000, "ms"),
    ],
  ),
  d(
    9,
    "闭环控制电机",
    "Control motor speed",
    "设定目标速度，让简化 PI 控制器调节模拟电机。改变负载，观察反馈恢复过程。",
    "A simplified PI controller drives a model motor toward the target. Change the load and watch feedback recover.",
    "PWM / ESC",
    "ESC 1 · Encoder 3/9",
    [
      p("target", "目标速度", "Target speed", 0, 3, 0.1, 1, "m/s"),
      p("load", "负载", "Load", 0, 100, 5, 20, "%"),
    ],
  ),
  d(
    10,
    "解算姿态",
    "Estimate attitude",
    "将模拟陀螺仪积分与重力角融合。调整倾角和融合权重，观察估计值逐步跟随。",
    "Blend integrated gyro motion with a gravity angle. Adjust tilt and blending weight to observe the estimate.",
    "IMU",
    "QMI8658 · AHRS",
    [
      p("angle", "目标倾角", "Target tilt", -80, 80, 1, 20, "°"),
      p("alpha", "融合权重", "Blend weight", 0.8, 0.99, 0.01, 0.95),
    ],
  ),
  d(
    11,
    "找到方向",
    "Find your heading",
    "磁力计通过水平磁场求航向。加入硬铁偏移，比较原始航向与校正后的航向。",
    "Compute heading from a horizontal magnetic field. Add a hard-iron offset and compare raw and corrected headings.",
    "IMU",
    "QMC6309 · I²C",
    [
      p("heading", "真实航向", "Heading", 0, 359, 1, 45, "°"),
      p("offset", "X 轴偏移", "X offset", -50, 50, 1, 10, "µT"),
    ],
  ),
  d(
    12,
    "组合成机器人",
    "Build a robot",
    "把遥控、速度反馈和转向连接起来。触发信号丢失，观察模拟车辆立即停止。",
    "Combine remote input, speed feedback and steering. Trigger signal loss to stop the simulated vehicle.",
    "ESP32-S3",
    "SBUS → Control → PWM",
    [
      p("target", "速度指令", "Speed command", 0, 3, 0.1, 1, "m/s"),
      p("steering", "转向", "Steering", -45, 45, 1, 0, "°"),
      p("failsafe", "信号丢失", "Signal lost", 0, 1, 1, 0),
    ],
  ),
  d(
    13,
    "出厂功能检查",
    "Factory check",
    "依次检查灯、蜂鸣器、传感器和输入输出。这里显示的是模拟检查结果，实机诊断请使用出厂固件。",
    "Step through LED, buzzer, sensor and I/O checks. These are simulated results; use factory firmware for real diagnostics.",
    "ESP32-S3",
    "Factory demo",
    [p("fault", "注入传感器故障", "Sensor fault", 0, 1, 1, 0)],
  ),
];
export const initialValues = (id) =>
  Object.fromEntries(lessons[id].params.map((p) => [p.key, p.value]));
export const createState = () => ({
  value: 0,
  integral: 0,
  count: 0,
  lastA: -1,
  lastB: -1,
  saved: null,
});
const fmt = (n, d = 2) => Number(n).toFixed(d);
export function lessonCode(id, v) {
  switch (id) {
    case 0:
      return [
        "// Startup sequence",
        "board_init();",
        "usb_console_init();",
        'ESP_LOGI("boot", "OSRCORE ready");',
      ];
    case 1:
      return [
        "// RMT helpers · Teaching excerpt",
        `const int interval_ms = ${v.interval};`,
        `const int brightness = ${v.brightness};`,
        "while (1) {",
        "  led_set(brightness, 0, 0);",
        "  vTaskDelay(pdMS_TO_TICKS(interval_ms));",
        "  led_set(0, brightness, 0);",
        "  vTaskDelay(pdMS_TO_TICKS(interval_ms));",
        "  led_set(0, 0, brightness);",
        "  vTaskDelay(pdMS_TO_TICKS(interval_ms));",
        "  led_set(0, 0, 0);",
        "  vTaskDelay(pdMS_TO_TICKS(interval_ms));",
        "}",
      ];
    case 2:
      return [
        "// LEDC helper · Teaching excerpt",
        `const int frequency_hz = ${v.frequency};`,
        `const int duration_ms = ${v.duration};`,
        "buzzer_tone(frequency_hz, duration_ms);",
        "vTaskDelay(pdMS_TO_TICKS(duration_ms + 100));",
      ];
    case 3:
      return [
        "// 50 Hz · 14-bit PWM",
        `int pulse_us = ${v.pulse};`,
        "int duty = pulse_us * 16383 / 20000;",
        "ledc_set_duty(LEDC_LOW_SPEED_MODE, CH_STEERING, duty);",
        "ledc_update_duty(LEDC_LOW_SPEED_MODE, CH_STEERING);",
      ];
    case 4:
      return [
        "// simulated receiver input",
        `int channel = ${v.channel};`,
        `bool failsafe = ${!!v.failsafe};`,
        "int pulse_us = failsafe ? 1500 :",
        "  1000 + (channel - 240) * 1000 / 1570;",
        "apply_pwm(pulse_us);",
      ];
    case 5:
      return [
        "// ideal gravity model",
        `float tilt_deg = ${v.angle};`,
        "float angle = tilt_deg * PI / 180;",
        "float ax = sinf(angle);",
        "float az = cosf(angle);",
        'ESP_LOGI("imu", "ax=%.2f az=%.2f g", ax, az);',
      ];
    case 6:
      return [
        "// Encoder count → RPM",
        `float counts_per_second = ${v.pps};`,
        `float counts_per_rev = ${v.ppr};`,
        "float rpm = counts_per_second * 60 / counts_per_rev;",
        'ESP_LOGI("encoder", "rpm=%.2f", rpm);',
      ];
    case 7:
      return [
        "// NVS helpers · Teaching excerpt",
        `float kp = ${v.kp};`,
        'nvs_set_blob(handle, "kp", &kp, sizeof(kp));',
        "nvs_commit(handle);",
        "// read after restart",
        'nvs_get_blob(handle, "kp", &kp, &size);',
      ];
    case 8:
      return [
        "// two periodic tasks",
        `const int task_a_ms = ${v.taskA};`,
        `const int task_b_ms = ${v.taskB};`,
        "// Task A",
        "sample_sensor(); // then delay(task_a_ms)",
        "// Task B",
        "publish_status(); // then delay(task_b_ms)",
      ];
    case 9:
      return [
        "// simplified PI plant",
        `float target = ${v.target};`,
        `float load = ${v.load / 100};`,
        "float error = target - speed;",
        "integral += error * dt;",
        "float output = clamp(2 * error + integral, 0, 3);",
        "speed += (output - speed - load) * dt * 2;",
      ];
    case 10:
      return [
        "// complementary filter",
        `float target_tilt = ${v.angle};`,
        `float alpha = ${v.alpha};`,
        "estimate = alpha * estimate +",
        "  (1 - alpha) * target_tilt;",
        'ESP_LOGI("ahrs", "roll=%.2f", estimate);',
      ];
    case 11:
      return [
        "// horizontal field",
        `float heading_deg = ${v.heading};`,
        `float hard_iron_x = ${v.offset};`,
        "float raw_x = 40 * cosf(rad(heading_deg)) + hard_iron_x;",
        "float raw_y = 40 * sinf(rad(heading_deg));",
        "float yaw = atan2f(raw_y, raw_x - hard_iron_x);",
      ];
    case 12:
      return [
        "// safety behaviour",
        `float command = ${v.target};`,
        `float steering = ${v.steering};`,
        `bool signal_lost = ${!!v.failsafe};`,
        "float target = signal_lost ? 0 : command;",
        "update_motor(target);",
        "update_servo(signal_lost ? 0 : steering);",
      ];
    default:
      return [
        "// simulated diagnostics",
        "check_rgb_led();",
        "check_buzzer();",
        `bool sensor_fault = ${!!v.fault};`,
        "check_sensor(sensor_fault);",
        "check_io();",
        "report_results();",
      ];
  }
}
export function stepSimulation(id, v, time, dt, state) {
  let value = 0,
    unit = "",
    label = "",
    line = 1,
    color = "#18212d",
    log = "",
    secondary = "",
    angle = 0;
  const sec = time / 1000;
  switch (id) {
    case 0:
      value = Math.min(3, Math.floor(time / 600));
      label = ["BOOT", "CPU", "USB", "READY"][value];
      line = value;
      log = label;
      break;
    case 1: {
      let phase = Math.floor(time / v.interval) % 4;
      let level = Math.round(v.brightness);
      const rgb = [
        [level, 0, 0],
        [0, level, 0],
        [0, 0, level],
        [0, 0, 0],
      ][phase];
      color = `rgb(${rgb.map((c) => (c ? Math.round(80 + (c * 175) / 255) : 0)).join(",")})`;
      value = phase === 3 ? 0 : level;
      unit = "/ 255";
      label = ["RED", "GREEN", "BLUE", "OFF"][phase];
      line = 4 + phase * 2;
      log = `RGB ${label} · ${rgb.join(", ")}`;
      secondary = `${v.interval} ms`;
      break;
    }
    case 2: {
      const on = time % (v.duration + 100) < v.duration;
      value = on ? v.frequency : 0;
      unit = "Hz";
      label = on ? "TONE" : "PAUSE";
      line = on ? 3 : 4;
      log = `${label} ${value} Hz`;
      secondary = "GPIO 42 · 50% PWM";
      break;
    }
    case 3:
      value = v.pulse;
      unit = "µs";
      label = "PWM";
      angle = ((v.pulse - 1500) / 500) * 90;
      line = 3 + (Math.floor(sec * 2) % 2);
      secondary = `${fmt(v.pulse / 200, 1)}% · ${Math.round((v.pulse * 16383) / 20000)} / 16383`;
      log = `PWM ${v.pulse} µs`;
      break;
    case 4:
      value = v.failsafe
        ? 1500
        : Math.round(1000 + ((v.channel - 240) * 1000) / 1570);
      unit = "µs";
      label = v.failsafe ? "FAILSAFE" : "SBUS";
      line = v.failsafe ? 3 : 5;
      secondary = `CH1 ${v.channel}`;
      log = `${label} → ${value} µs`;
      angle = (value - 1500) * 0.18;
      break;
    case 5:
      value = Math.sin((v.angle * Math.PI) / 180);
      unit = "g";
      label = "ACCEL X";
      angle = v.angle;
      line = 3 + (Math.floor(sec * 2) % 3);
      secondary = `Z ${fmt(Math.cos((v.angle * Math.PI) / 180))} g`;
      log = `ax=${fmt(value)}g az=${fmt(Math.cos((v.angle * Math.PI) / 180))}g`;
      break;
    case 6:
      state.count += v.pps * dt;
      value = (v.pps * 60) / v.ppr;
      unit = "RPM";
      label = "ENCODER";
      line = 3;
      angle = ((state.count / v.ppr) * 360) % 360;
      secondary = `COUNT ${Math.round(state.count)}`;
      log = `count=${Math.round(state.count)} rpm=${fmt(value)}`;
      break;
    case 7:
      value = v.kp;
      label = "NVS";
      line = 2 + (Math.floor(sec) % 4);
      secondary = state.saved == null ? "UNSAVED" : `SAVED ${state.saved}`;
      log = state.saved == null ? "Awaiting Save" : `NVS kp=${state.saved}`;
      break;
    case 8: {
      const a = Math.floor(time / v.taskA),
        b = Math.floor(time / v.taskB);
      const aa = a !== state.lastA,
        bb = b !== state.lastB;
      state.lastA = a;
      state.lastB = b;
      value = a + b;
      label = aa && bb ? "A + B" : aa ? "TASK A" : bb ? "TASK B" : "WAIT";
      line = aa ? 4 : bb ? 6 : 1;
      log = aa || bb ? `${label} · ${Math.round(time)} ms` : "";
      secondary = `A ${a} · B ${b}`;
      break;
    }
    case 9: {
      const error = v.target - state.value;
      state.integral = Math.max(-3, Math.min(3, state.integral + error * dt));
      const output = Math.max(0, Math.min(3, 2 * error + state.integral));
      state.value = Math.max(
        0,
        state.value + (output - state.value - v.load / 100) * dt * 2,
      );
      value = state.value;
      unit = "m/s";
      label = "SPEED";
      line = 3 + (Math.floor(sec * 4) % 4);
      secondary = `TARGET ${fmt(v.target)} m/s`;
      log = `target=${fmt(v.target)} speed=${fmt(value)}`;
      break;
    }
    case 10:
      state.value =
        Math.pow(v.alpha, dt / 0.05) * state.value +
        (1 - Math.pow(v.alpha, dt / 0.05)) * v.angle;
      value = state.value;
      unit = "°";
      angle = value;
      label = "ROLL";
      line = 3 + (Math.floor(sec * 2) % 3);
      secondary = `TARGET ${v.angle}°`;
      log = `roll=${fmt(value)}°`;
      break;
    case 11: {
      const rad = (v.heading * Math.PI) / 180,
        x = 40 * Math.cos(rad) + v.offset,
        y = 40 * Math.sin(rad);
      value = v.heading;
      unit = "°";
      angle = value;
      label = "HEADING";
      line = 3 + (Math.floor(sec * 2) % 3);
      secondary = `RAW ${fmt(((Math.atan2(y, x) * 180) / Math.PI + 360) % 360, 1)}°`;
      log = `raw=${secondary.slice(4)} corrected=${value}°`;
      break;
    }
    case 12:
      state.value = v.failsafe
        ? 0
        : state.value + (v.target - state.value) * Math.min(1, dt * 3);
      value = state.value;
      unit = "m/s";
      angle = v.failsafe ? 0 : v.steering;
      label = v.failsafe ? "STOPPED" : "ROBOT";
      line = v.failsafe ? 4 : 5 + (Math.floor(sec * 2) % 2);
      secondary = `STEER ${angle}°`;
      log = `${label} ${fmt(value)} m/s · ${angle}°`;
      break;
    case 13: {
      const phase = Math.floor(sec) % 5;
      value = phase + 1;
      unit = "/ 5";
      label = ["RGB", "BUZZER", "SENSOR", "I/O", "RESULT"][phase];
      line = phase + 1;
      secondary = v.fault && phase >= 2 ? "SENSOR ERROR" : "SIMULATED PASS";
      color = phase === 0 ? "#0076df" : "#18212d";
      log = `${label}: ${secondary}`;
      break;
    }
  }
  return { value, unit, label, line, color, log, secondary, angle };
}
