# ESP32 WiFi 空口分析器 — 项目作业要求

## 一、项目概述

利用 **单个 ESP32 开发板**（零外设、零传感器），实现一个 **WiFi 空口分析器**。

ESP32 进入 WiFi 混杂模式（Promiscuous Mode），监听周围所有 2.4GHz WiFi 信号，解析 802.11 帧，并通过内嵌的 Web 服务器在浏览器上实时展示 WiFi 环境信息。

**不需要任何额外硬件。** 仅需：ESP32 开发板 × 1、USB 线 × 1、能打开浏览器的电脑 × 1。

---

## 二、最终效果

打开浏览器访问 ESP32 的 IP 地址，看到实时的 WiFi 环境仪表盘：

```
信道 1  ━━━━━━━━━━━━━━  12 个 AP
信道 6  ━━━━  5 个 AP
信道 11 ━━━━━━━━━━━━━━━━━━  18 个 AP

┌──────────────────────────────────────────────┐
│ SSID              MAC              RSSI  信道 │
│ Xiaomi_ABCD       XX:XX:XX:XX:XX   -45    6  │
│ TP-Link_DEAD      XX:XX:XX:XX:XX   -62    1  │
│ CMCC-5G           XX:XX:XX:XX:XX   -71   11  │
│ …                                            │
└──────────────────────────────────────────────┘

实时抓包速率: 124 pkt/s    丢弃: 0    已发现 AP: 23
```

---

## 三、技术栈

| 层面       | 要求                                         |
|-----------|---------------------------------------------|
| 芯片       | ESP32（任何型号均可）                          |
| 开发框架   | **ESP-IDF**（乐鑫官方框架，**禁止使用 Arduino**）|
| 抓包       | WiFi Promiscuous Mode + 802.11 帧手动字节解析  |
| 操作系统   | FreeRTOS（ESP-IDF 自带）                      |
| 网络协议栈 | lwIP（ESP-IDF 自带）                          |
| 文件系统   | SPIFFS（存放网页文件）                         |
| 网页前端   | 纯原生 HTML + CSS + JavaScript + Canvas       |
| 实时通信   | WebSocket                                    |

---

## 四、功能要求（按阶段）

### 阶段一：裸机抓包输出到串口

1. **开启 WiFi Promiscuous Mode**
   - 调用 `esp_wifi_set_promiscuous(true)`
   - ESP32 不连接任何路由器，只收不发

2. **注册接收回调**
   - `esp_wifi_set_promiscuous_rx_cb()` 注册回调函数
   - 每次收到 WiFi 帧，回调被调用
   - 回调参数：帧原始数据指针 + 长度 + RSSI + 速率等信息

3. **手动解析 802.11 帧头**
   - **必须手动解析字节**，禁止直接使用现成的协议解析库（如 tcpdump/libpcap 移植）
   - 需要解析的字段：
     - **Frame Control**（2 字节）：帧类型（管理帧 0x00 / 数据帧 0x02 / 控制帧 0x01）
     - **Duration**（2 字节）
     - **Address 1 / Address 2 / Address 3**（各 6 字节）：目的 MAC、源 MAC、BSSID
     - **Sequence Control**（2 字节）：提取 Sequence Number
   - 用 `struct __attribute__((packed))` 或逐字节位移读取

4. **识别并解析 Beacon 帧**
   - Beacon 帧是路由器定期广播的管理帧（Frame Control = 0x80）
   - 从 Beacon 帧的 Tagged Parameters 中提取：
     - **SSID**（Tag 0x00）
     - **Supported Rates**（Tag 0x01）
     - **DS Parameter Set / 信道**（Tag 0x03）
   - 串口打印格式：`[BEACON] SSID: Xiaomi_ABCD  BSSID: XX:XX:XX:XX:XX  RSSI: -45 dBm  CH: 6`

5. **验收标准**
   - 插上 ESP32，打开串口监视器（115200 baud）
   - 串口持续滚动打印周围 WiFi 环境信息
   - 能看到至少 3 个不同路由器的 Beacon

---

### 阶段二：信道扫描 + AP 管理

1. **13 信道自动轮询**
   - 2.4GHz WiFi 信道 1~13
   - 每个信道停留 **200ms**，然后切换到下一个
   - 循环扫描
   - 信道切换：`esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE)`

2. **AP 信息表维护**
   - 用链表或数组维护已发现的路由器
   - 每条记录包含：
     - SSID（字符串）
     - BSSID（6 字节 MAC）
     - RSSI（最新信号强度）
     - 信道
     - 首次发现时间
     - 最后一次见到的时间戳
   - 相同 BSSID 的 AP 只存一条，更新 RSSI
   - 超过 **30 秒** 未出现的 AP 从列表中移除

3. **统计信息**
   - 每个信道上的 AP 数量
   - 累计抓包总数
   - 每秒抓包速率

4. **验收标准**
   - 串口每秒打印一次环境摘要
   - 拿着 ESP32 走动时，AP 列表能动态变化

---

### 阶段三：Web 服务器 + 实时浏览器展示

1. **SPIFFS 文件系统**
   - 配置分区表，分配 SPIFFS 分区（建议 512KB 以上）
   - 将 HTML + CSS + JavaScript 文件打包为 SPIFFS 镜像
   - 烧录到 Flash，ESP32 启动时挂载

2. **HTTP 服务器**
   - 使用 ESP-IDF 的 `httpd` 组件
   - 实现以下路由：

| 路径 | 方法 | 说明 |
|------|------|------|
| `/` | GET | 返回 index.html（仪表盘首页） |
| `/api/wifi/list` | GET | 返回 JSON：当前所有 AP 的完整列表 |
| `/api/wifi/stats` | GET | 返回 JSON：各信道 AP 数量 + 总统计 |

3. **JSON 格式**

`GET /api/wifi/list` 返回：
```json
{
  "aps": [
    {
      "ssid": "Xiaomi_ABCD",
      "bssid": "xx:xx:xx:xx:xx:xx",
      "rssi": -45,
      "channel": 6,
      "first_seen": 12345,
      "last_seen": 12789
    }
  ]
}
```

`GET /api/wifi/stats` 返回：
```json
{
  "channel_distribution": [0, 5, 0, 0, 0, 12, 0, 0, 0, 0, 18, 0, 0],
  "total_aps": 35,
  "total_packets": 102400,
  "packets_per_second": 124,
  "dropped_packets": 0
}
```

4. **WebSocket 实时推送**
   - 在 HTTP 服务器上开启 WebSocket 端点 `/ws`
   - ESP32 每秒主动推送一次最新的 `list` + `stats` 合并数据
   - 浏览器端 WebSocket 连接，收到数据后自动更新页面

5. **前端页面（index.html）**
   - **零依赖**：只用原生 HTML + CSS + JavaScript，不使用 React/Vue/Bootstrap/jQuery
   - 页面包含：
     - 信道分布条形图（用 Canvas 绘制）
     - AP 列表表格（SSID / BSSID / RSSI / 信道 / 首次发现时间）
     - 实时统计卡片（总 AP 数 / 抓包速率 / 丢弃包数）
   - 页面样式整洁，移动端和 PC 端均可查看

6. **验收标准**
   - ESP32 上电后，电脑连接同一局域网（或 ESP32 作为 AP）
   - 浏览器输入 ESP32 的 IP 地址，看到实时更新的 WiFi 仪表盘
   - 页面每秒钟自动更新

---

### 阶段四：高级功能（加分项）

1. **Deauth 攻击检测**
   - Deauth 帧（Frame Control = 0xC0）是踢设备下线的管理帧
   - 统计每秒收到的 Deauth 帧数量
   - 如果同一 BSSID 的 Deauth 帧速率超过阈值（如 10 pkt/s），判定为攻击
   - 页面显示红色闪烁警告

2. **RSSI 历史曲线**
   - 对每个 AP 记录最近 60 秒的 RSSI 数据
   - 在网页上用 Canvas 折线图显示信号强度变化趋势

3. **信道利用率统计**
   - 统计每个信道上信道忙的时间占比
   - 显示最拥堵的信道排名
   - 提示用户切换到最优信道

4. **验收标准**
   - Deauth 攻击时页面弹出警告
   - 点击 AP 可查看 RSSI 历史曲线

---

## 五、禁止事项

| ❌ 禁止 | ✅ 应该做 |
|--------|----------|
| 使用 Arduino 框架（包括 Arduino-ESP32） | 使用 **ESP-IDF** 原生框架 |
| 调用现成的 802.11 帧解析库 | **手动逐字节解析** 802.11 帧头 |
| 使用第三方前端框架（React/Vue/Bootstrap） | 原生 HTML + CSS + JS + Canvas |
| 在回调函数或 ISR 中做大量处理 | 回调中快速拷贝数据到队列，处理放在任务中 |
| 在中断中调用 `printf` / `malloc` | 中断中只做最小操作 |
| 全局变量满天飞 | 用结构体封装状态，任务间用队列通信 |

---

## 六、项目交付物

1. **完整 ESP-IDF 工程源码**（目录结构如下）

```
wifi_sniffer/
├── CMakeLists.txt
├── sdkconfig
├── partitions.csv
├── main/
│   ├── CMakeLists.txt
│   ├── main.c              // 入口 + WiFi 初始化 + 任务创建
│   ├── wifi_sniffer.c      // Promiscuous 模式 + 帧解析
│   ├── wifi_sniffer.h
│   ├── ap_table.c          // AP 信息表管理
│   ├── ap_table.h
│   ├── web_server.c        // HTTP + WebSocket 服务器
│   └── web_server.h
├── server/
│   ├── index.html          // 仪表盘前端
│   ├── style.css
│   └── script.js
└── README.md               // 项目说明文档
```

2. **README.md** 包含：
   - 项目简介
   - 硬件需求
   - 构建与烧录步骤（`idf.py build` / `idf.py flash` / `idf.py monitor`）
   - 使用时注意事项
   - 项目结构说明
   - 核心实现思路（可选）

3. **演示视频**（可选加分）
   - 30~60 秒
   - 展示 ESP32 上电 → 浏览器打开仪表盘 → 实时数据更新

---

## 七、评分标准

| 项目 | 分值 | 说明 |
|------|------|------|
| 阶段一：串口抓包输出 | 20 | 正确解析 Beacon 帧并打印 |
| 阶段二：信道扫描 + AP 管理 | 15 | 13 信道轮询、AP 表正确维护 |
| 阶段三：Web 服务器 + 前端 | 30 | HTTP + WebSocket + 前端仪表盘完整可用 |
| 代码质量 | 15 | 结构清晰、命名规范、有注释、无魔法数字 |
| README 文档 | 10 | 完整、清晰、能照着步骤复现 |
| 高级功能（加分） | 10 | Deauth 检测 / RSSI 曲线 / 信道利用率 |
| **总分** | **100** | |

---

## 八、学习资源

| 资源 | 链接 |
|------|------|
| ESP-IDF 编程指南 | https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32/ |
| ESP-IDF WiFi API 参考 | https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html |
| 802.11 帧格式参考 | https://en.wikipedia.org/wiki/802.11_Frame |
| ESP-IDF HTTP Server | ESP-IDF 示例: `examples/protocols/http_server/` |
| ESP-IDF WebSocket | ESP-IDF 示例: `examples/protocols/http_server/ws_echo_server/` |
| SPIFFS 使用 | ESP-IDF 示例: `examples/storage/spiffs/` |

---

## 九、建议时间安排

| 阶段 | 建议用时 |
|------|---------|
| 环境搭建（ESP-IDF 安装 + Hello World） | 1 天 |
| 阶段一：裸机抓包 | 3~4 天 |
| 阶段二：信道扫描 + AP 管理 | 2~3 天 |
| 阶段三：Web 服务器 + 前端 | 5~7 天 |
| 阶段四：高级功能（加分） | 3~5 天 |
| README + 演示视频 | 1 天 |
| **总计** | **约 3~4 周** |

---

## 十、常见问题

**Q：ESP32 会连接 WiFi 吗？**
A：不会。Promiscuous Mode 下 ESP32 只收不发，不连接任何路由器。

**Q：电脑怎么访问 ESP32 的网页？**
A：两种方式：
1. ESP32 作为 SoftAP 开热点，电脑连上后访问 `192.168.4.1`
2. ESP32 连接你的路由器（Station 模式），在同一局域网下访问其 IP

**Q：为什么不用 Arduino？**
A：Arduino 屏蔽了底层细节。ESP-IDF 让你直接操作 WiFi 驱动、FreeRTOS API、lwIP 协议栈——这才是嵌入式工程师该做的事。

**Q：内存够吗？**
A：ESP32 有 520KB RAM。合理安排：Ringbuffer 约 10KB，AP 表约 5KB，HTTP/WebSocket 连接约 20KB，堆余量 200KB+，完全够用。

**Q：抓到的包怎么存？**
A：用 Ringbuffer（环形缓冲区）暂存 WiFi 帧，抓包线程写入，处理线程读取。避免频繁 malloc/free。

---

## 十一、核心工程挑战

| 挑战 | 说明 |
|------|------|
| 802.11 帧手动解析 | 理解 Frame Control 位域、Address 字段顺序、Tagged Parameters 格式 |
| 双任务设计 | 抓包任务优先级 vs HTTP 服务任务优先级，怎么分？用什么 IPC？ |
| Ringbuffer 设计 | 缓冲区大小？写满时丢弃旧包还是新包？原子操作还是关中断？ |
| AP 表超时淘汰 | 遍历链表删除过期节点，注意多任务访问互斥 |
| WebSocket JSON 构建 | ESP32 上手动 sprintf 拼 JSON（推荐使用 cJSON 库管理 JSON） |
| 前端 Canvas 绘图 | 原生 JS 操作 Canvas API 画条形图和折线图 |

---

*项目设计：Reasonix Code*