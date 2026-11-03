// Shared by the teaching UI and local firmware compiler. Never accept arbitrary macros or paths.
export const exampleDirs=['','01_blink_led','02_buzzer','03_pwm_servo','04_sbus_rc','05_imu_i2c','06_encoder','07_nvs','08_freertos','09_pid_motor','10_ahrs','11_mag_compass','12_full_example','13_factory_demo'];
const cfg=(macro,min,max,step,value)=>({macro,min,max,step,value});
export const firmwareParameters={
 1:{interval:cfg('LED_INTERVAL',100,2000,50,500),brightness:cfg('LED_BRIGHTNESS',0,255,1,32)},
 2:{frequency:cfg('BUZZER_FREQUENCY',220,1760,10,440),duration:cfg('BUZZER_DURATION',100,1000,50,300)},
 3:{pulse:cfg('SERVO_PULSE',1000,2000,10,1500)},
 6:{ppr:cfg('ENCODER_CPR',100,20000,100,10800)},
 7:{kp:cfg('NVS_KP',0,20,.1,2.5)},
 8:{taskA:cfg('TASK_A_MS',100,1500,100,500),taskB:cfg('TASK_B_MS',100,1500,100,1000)},
 10:{alpha:cfg('AHRS_ALPHA',.8,.99,.01,.95)},
};
export function validateFirmwareRequest(lesson,values={}){
 if(!Number.isInteger(lesson)||lesson<1||lesson>=exampleDirs.length)throw Error('请选择可下载的课程');
 if(!values||typeof values!=='object'||Array.isArray(values))throw Error('参数格式错误');
 const definitions=firmwareParameters[lesson]||{},parameters={};
 for(const [key,definition] of Object.entries(definitions)){
  const value=Object.hasOwn(values,key)?values[key]:definition.value;
  if(typeof value!=='number'||!Number.isFinite(value)||value<definition.min||value>definition.max)throw Error(`参数 ${key} 超出允许范围`);
  const steps=(value-definition.min)/definition.step;
  if(Math.abs(steps-Math.round(steps))>1e-6)throw Error(`参数 ${key} 不符合步长 ${definition.step}`);
  parameters[key]=Number(value.toFixed(4));
 }
 const unknown=Object.keys(values).filter(key=>!Object.hasOwn(definitions,key));
 if(unknown.length)throw Error(`这些参数仅用于网页模拟，不能写入固件：${unknown.join(', ')}`);
 return {lesson,example:exampleDirs[lesson],parameters};
}
export function firmwareValues(lesson,values){return Object.fromEntries(Object.keys(firmwareParameters[lesson]||{}).map(k=>[k,values[k]]));}
export function configHeader(lesson,parameters){return '#pragma once\n/* Generated from the reviewed lesson parameter contract. */\n'+Object.entries(parameters).map(([k,v])=>`#define OSR_${firmwareParameters[lesson][k].macro} ${v}`).join('\n')+'\n';}
export const lessonGuide=[
 {goal:'认识板卡与接口',action:'点选一个元件，查看接口线序；拖动查看背面。',result:'右侧显示所选元件的引脚与功能。',hardware:'不需要外设。此页用于认识硬件，不下载启动示意代码。'},
 {goal:'让一颗灯按自己的节奏闪烁',action:'调整亮度和间隔，运行红、绿、蓝、灭四个状态。',result:'灯色每隔指定时间切换；亮度为 0 时保持熄灭。',hardware:'仅开发板和 USB 数据线。'},
 {goal:'用频率控制音高',action:'调整频率与时长。每次发声后留出 100 ms 静音。',result:'频率越高声音越尖；时长决定每次响多久。',hardware:'板载无源蜂鸣器 GPIO42；网页声音需在设置中开启。'},
 {goal:'给舵机一个明确的位置指令',action:'调整 GPIO2 的 50 Hz PWM 脉宽。先用 1500 µs 中位。',result:'真板输出固定脉宽；网页角度仅为常见舵机近似。',hardware:'EX2 第 8 行：GPIO2 / VCC_ESC / GND。独立供电并共地；GPIO1 始终保持 1500 µs。'},
 {goal:'看懂接收机的数据和失联标志',action:'网页拖动模拟通道；真板连接 SBUS 接收机并操作遥控器。',result:'观察通道数据、帧丢失与失联状态。',hardware:'EX2 第 4 行：GPIO44 / 5 V / GND；仅接兼容的 SBUS 信号。网页通道值不写入固件。'},
 {goal:'让板卡测量自己的运动',action:'网页改变模拟倾角；真板启动时保持静止，随后轻轻转动。',result:'查看三轴加速度、角速度和温度；静置时加速度模长接近 1 g。',hardware:'板载 QMI8658；GPIO10/11。模拟倾角不是给真板的控制指令。'},
 {goal:'把编码器计数换算为转速',action:'设置实际输出轴每转计数，再转动编码器。',result:'RPM = 每秒计数 ÷ 每转计数 × 60；方向改变时符号改变。',hardware:'EX4：GPIO3 = A，GPIO9 = B，共地；每转计数需按单边沿计数方式填写。'},
 {goal:'断电后仍能读回一个参数',action:'设置 Kp 并下载，发送 save 保存；重启后读日志。',result:'看到 Saved 和 Loaded 的数值一致；仅改滑条不会覆盖已保存值。',hardware:'不需要外设。使用独立 lesson7 命名空间，不清空整片 Flash。'},
 {goal:'观察两个任务独立执行',action:'分别设置 A、B 的周期并运行，比较打印次数。',result:'两项任务按各自周期输出；网页时间轴用于理解周期，不模拟 CPU 调度细节。',hardware:'不需要外设；使用 vTaskDelayUntil 保持任务周期。'},
 {goal:'理解编码器反馈如何调节电机',action:'先在网页改变目标与模拟负载，再阅读真板控制环。',result:'观察目标、反馈和输出；真板上电保持中位，控制命令超时 1 秒归零。',hardware:'进阶：电调 GPIO1、编码器 GPIO3/9、独立供电。真板需持续发送 v <m/s>；停止用 stop。'},
 {goal:'融合陀螺仪与重力方向',action:'调整融合权重，比较角度估计的响应。真板静置校准后再转动。',result:'看到 Roll/Pitch 随动作变化；本课不估计无参考的绝对航向。',hardware:'板载 QMI8658；互补滤波，默认不启用加热器。'},
 {goal:'理解磁力计与航向校正',action:'网页加入模拟磁偏移；真板水平旋转并按源码进行采样校准。',result:'比较原始与校准后的航向；远离磁铁与大电流导线。',hardware:'板载 QMC6309，I²C GPIO10/11；网页磁场和偏移仅为模拟输入。'},
 {goal:'把遥控、测量和控制串起来',action:'先完成前面的输入、传感器和输出课程，再观察组合流程。',result:'网页失联时停机；真板遥控超时归零，默认不等待加热。',hardware:'综合进阶：接收机、编码器、舵机及电调；网页车辆是行为模型，实际数据以设备日志为准。'},
 {goal:'检查板载功能是否响应',action:'下载诊断例程，逐项查看日志。网页故障开关仅用于认识失败分支。',result:'明确区分真实检测结果与网页模拟结果。',hardware:'板载灯、蜂鸣器、传感器；外部接口测试需接相应设备。'},
];

export const lessonHardwareEn=["No peripherals required. Explore the controller; this page does not flash startup pseudocode.", "Controller and USB data cable only.", "Onboard passive buzzer: GPIO42. Enable browser audio in Settings.", "EX2 row 8: GPIO2 / VCC_ESC / GND. Use a separate supply and common ground. GPIO1 stays at 1500 \u00b5s.", "EX2 row 4: GPIO44 / 5 V / GND. Use a compatible SBUS receiver. Browser channel values are not compiled into firmware.", "Onboard QMI8658 on GPIO10/11. Simulated angles are not commands to the physical controller.", "EX4: GPIO3=A, GPIO9=B, common ground. Set counts per output-shaft revolution for single-edge counting.", "No peripherals. Uses the isolated lesson7 NVS namespace and does not erase the entire flash.", "No peripherals. Uses vTaskDelayUntil for periodic execution.", "Advanced: ESC on GPIO1, encoder on GPIO3/9 and an independent supply. Send v <m/s> continuously; stop halts output.", "Onboard QMI8658. Complementary filtering; heater disabled by default.", "Onboard QMC6309 on I\u00b2C GPIO10/11. Browser magnetic fields and offsets are simulated inputs.", "Advanced integration: receiver, encoder, servo and ESC. The animation is a behavior model; device logs report real measurements.", "Onboard LED, buzzer and sensors. Connect external peripherals to test their interfaces."];
