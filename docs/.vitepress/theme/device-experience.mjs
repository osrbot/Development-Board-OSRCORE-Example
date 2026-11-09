import {exampleDirs} from './lesson-contract.mjs';

// A serial connection or a successful flash is not an application acknowledgement.
export function parseDeviceInfo(line){
 if(!line.startsWith('OSRCORE_INFO '))return null;
 try{const info=JSON.parse(line.slice(13));
  if(info.board!=='OSRCORE'||info.protocol!==1||typeof info.project!=='string'||typeof info.version!=='string')return null;
  return {board:info.board,protocol:1,project:info.project,version:info.version,lesson:exampleDirs.indexOf(info.project),...(info.runtimeParameters===1?{runtimeParameters:1,parameters:info.parameters,parametersSaved:info.parametersSaved===true}:{})};
 }catch{return null;}
}
export function deviceProblem(message){
 const text=String(message);
 if(/SHA|MD5|hash|checksum|校验失败|verification failed/i.test(text))return 'verification';
 if(/fetch|HTTP|network|manifest|固件暂时|firmware.*unavailable/i.test(text))return 'firmware';
 if(/permission|denied|security|notallowed/i.test(text))return 'permission';
 if(/locked|in use|already open|failed to open|close timed out/i.test(text))return 'occupied';
 if(/ESP32-S3|chip|芯片|仅支持/i.test(text))return 'chip';
 return 'connection';
}
export const troubleshooting={
 connection:{zh:['设备没有连接或没有响应','确认使用数据线并接稳 USB。若设备列表为空，重新插拔；只有无法进入下载模式时，才使用下方 BOOT 恢复步骤。'],en:['Device missing or unresponsive','Check the USB data cable and reconnect it if the device list is empty. Use BOOT recovery below only when download mode cannot be entered.']},
 occupied:{zh:['USB 正被占用','关闭其他网页串口、串口助手或 IDE 的串口监视器，再重新连接。'],en:['USB port is busy','Close other browser serial sessions, serial terminals or IDE monitors, then reconnect.']},
 permission:{zh:['浏览器未允许串口访问','使用桌面 Chrome 或 Edge 打开 HTTPS 正式站，点击连接，并在弹窗中选择开发板。'],en:['Serial access is not allowed','Open the HTTPS website in desktop Chrome or Edge, click Connect and select the controller in the browser prompt.']},
 firmware:{zh:['课程固件未能获取','检查网络，重新获取本课固件后再下载。获取失败不会开始写入。'],en:['Course firmware could not be loaded','Check your network and reload the lesson firmware. Flashing does not begin if firmware download fails.']},
 verification:{zh:['固件校验未通过','当前下载尚未确认成功。检查数据线并重新下载；不要把写入进度当作校验结果。'],en:['Firmware verification failed','This download is not confirmed. Check the data cable and retry; write progress alone does not confirm verification.']},
 chip:{zh:['设备型号不匹配','本课程固件仅用于 OSRCORE 的 ESP32-S3。请重新选择正确的开发板。'],en:['Controller does not match','These course builds target the OSRCORE ESP32-S3. Select the correct controller.']},
};
// Physical actions and observations, deliberately separate from simulation instructions.
export const physicalLessons=[null,
 {zh:['观察板载 RGB 灯。','灯按红、绿、蓝、熄灭循环；日志打印对应颜色。'],en:['Watch the onboard RGB LED.','It cycles through red, green, blue and off; the console prints each color.']},
 {zh:['听板载蜂鸣器，并查看日志。','蜂鸣器周期发声；日志显示 tone 和 duration。'],en:['Listen to the onboard buzzer and watch the console.','It sounds periodically; logs show tone and duration.']},
 {zh:['接好舵机独立供电和共地，再观察舵机。','GPIO2 输出固定脉宽，日志显示 steering；GPIO1 保持 1500 µs 中位。'],en:['Connect the servo with its supply and common ground, then observe it.','GPIO2 outputs a fixed pulse width shown as steering in logs; GPIO1 stays neutral at 1500 µs.']},
 {zh:['接入兼容的 SBUS 接收机，操作遥控器。','日志中的 ch 通道值随操作改变，同时报告丢帧和失联状态。'],en:['Connect a compatible SBUS receiver and move the transmitter controls.','The ch values change with the controls; logs also report lost frames and failsafe state.']},
 {zh:['启动时保持板子静止；校准完成后轻轻转动。','ax/ay/az、gx/gy/gz 和温度随实测更新。'],en:['Keep the board still at startup, then gently rotate it after calibration.','ax/ay/az, gx/gy/gz and temperature update with measured data.']},
 {zh:['接入编码器，手动转动输出轴。','count、delta 和 rpm 更新，反向转动会改变方向符号；转速换算取决于固件的每转计数。'],en:['Connect the encoder and turn its output shaft by hand.','count, delta and rpm update; reversing changes the sign. RPM depends on the counts-per-revolution value compiled into firmware.']},
 {zh:['调整 Kp，点击“应用到开发板”和“保存到开发板”，再重启查看日志。','保存确认值与重启后的 Loaded 数值一致；只改网页滑条不会改变实板。'],en:['Adjust Kp, click Apply to controller and Save to controller, then restart and inspect the console.','The confirmed saved value matches Loaded after restart. Moving a slider alone does not change the controller.']},
 {zh:['观察日志中 A、B 两个任务的输出。','task、count 和 tick 按各自周期更新。'],en:['Watch the console output from tasks A and B.','task, count and tick update at each task’s own interval.']},
 {zh:['先观察中位输出和编码器反馈。需要驱动时按本课接线说明操作。','日志显示 target、actual 和 pulse；控制指令超过 1 秒未更新会归零，stop 可停止。'],en:['First inspect neutral output and encoder feedback. Follow this lesson’s wiring instructions before driving.','Logs show target, actual and pulse. Commands expire after 1 second; stop halts output.']},
 {zh:['静置完成校准，再缓慢倾斜板子。','roll 和 pitch 随倾斜改变；本课不提供绝对航向。'],en:['Keep the board still for calibration, then tilt it slowly.','roll and pitch change with tilt; this lesson does not provide absolute heading.']},
 {zh:['远离磁铁和大电流导线，水平旋转板子；发送 help 查看校准命令。','日志显示 hdg、x、y、z；校准后再比较航向。'],en:['Rotate the board horizontally away from magnets and high-current cables; send help for calibration commands.','Logs show hdg, x, y and z. Compare heading after calibration.']},
 {zh:['按综合课程接线，先检查传感器与遥控状态，再进行控制。','启动日志显示 osrcore ready；遥控失联会归零。实板数据以设备日志为准。'],en:['Follow the integrated lesson wiring; check sensors and receiver status before controlling motion.','Startup logs show osrcore ready; receiver loss returns output to neutral. Use device logs for real measurements.']},
 {zh:['发送 diag，观察灯、蜂鸣器并检查诊断日志。','DIAG 输出传感器和接口检测结果；灯与蜂鸣器仍需目视和听音确认。'],en:['Send diag, watch the LED, listen to the buzzer and inspect diagnostics.','DIAG reports sensor and interface checks. Confirm LED and buzzer operation by observation.']},
];
