# 本地课程、编译与开发板

使用已安装的 ESP-IDF v6.0.1。默认路径为 `~/esp/esp-idf`，可通过 `OSRCORE_IDF_PATH` 指定。

```sh
npm --prefix docs ci
npm --prefix docs run build
node tools/lab-server.mjs
```

连接开发板的电脑使用 `http://localhost:5173/`；同一局域网浏览课程使用本机IPv4的5173端口。浏览器USB需要安全上下文与Web Serial支持；普通局域网HTTP不能直接调用USB。页面内Wi-Fi操作经本地服务转发到用户明确填写的局域网板卡IPv4。

1. 在课程页先运行网页模拟，观察参数。
2. 点击顶部“连接开发板”，选择USB设备。
3. 编译当前参数，完成后下载。课程或参数改变会使旧构建不可下载。
4. 重连USB读取启动日志。首次Wi-Fi使用，在USB页展开配网，输入SSID与密码，发送后读取配对信息。
5. 切换Wi-Fi页并连接，可下载当前构建的应用固件；重启后重新连接，核对运行程序和构建编号。

所有构建存放 `.cache/lesson-builds`，按源文件指纹、课程、参数生成编号；镜像附带SHA-256。USB不执行整片擦除，适用于当前项目分区布局；Wi-Fi使用OTA槽。网页不会在移动滑条时自动驱动真板。

```sh
node --test tools/*.test.mjs docs/scripts/*.test.mjs
node tools/check-firmware.mjs
node tools/verify-firmware-artifacts.mjs
```

本地编译服务不是Vercel静态站的一部分。后续公开部署需要另行设计受控编译服务或预编译固件分发；静态预览版仅开放课程、模型和资料；实板下载仍通过本地服务验收。
