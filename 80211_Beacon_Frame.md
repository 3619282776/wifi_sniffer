# 802.11 Beacon 帧结构

> 对应代码位置：`main/WIFI/wifi_driver.c`

---

## 一、整体布局

```
┌──────────────────────────────────────────────────────────────────────┐
│                       整个 WiFi 帧                                   │
├──────────────────────────────────────────────────────────────────────┤
│ wifi_promiscuous_pkt_t                                              │
│   ├── rx_ctrl (RSSI、速率、长度等信息)                                 │
│   └── payload → 指向下面整个数据                                      │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 二、分层详解

### 1️⃣ MAC 头 — 24 字节

对应你的 `ieee80211_frame_header_t` 结构体：

```
payload[0~23]
┌──────────┬──────────┬──────────────┬──────────────┬──────────────┬──────────┐
│   0~1    │   2~3    │    4~9       │   10~15      │   16~21      │  22~23   │
│  Frame   │ Duration │  Address 1   │  Address 2   │  Address 3   │ Sequence │
│ Control  │          │  (目的/RA)    │  (源/TA)      │  (BSSID)     │ Control  │
├──────────┴──────────┴──────────────┴──────────────┴──────────────┴──────────┤
│                                 24 bytes                                  │
└───────────────────────────────────────────────────────────────────────────┘
```

#### Frame Control 字段分解

```
uint16_t frame_ctrl (2 字节 = 16 bits)

┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐
│ 15 │ 14 │ 13 │ 12 │ 11 │ 10 │  9 │  8 │  7 │  6 │  5 │  4 │  3 │  2 │  1 │  0 │
│Order│Pro-│More│Pwr │More│Re- │Frag│ToDS│   Subtype    │  Type  │ Protocol │
│     │tect│Data│Mgmt│Data│try │    │    │              │        │  Version │
└────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘
```

**提取 Type 和 Subtype：**

| 代码 | 掩码 | 提取字段 | 取值范围 |
|------|------|---------|---------|
| `(hdr->frame_ctrl & 0x000C) >> 2` | 位 2~3 | **Type** (帧类型) | 0~3 |
| `(hdr->frame_ctrl & 0x00F0) >> 4` | 位 4~7 | **Subtype** (帧子类型) | 0~15 |

**Type 取值：**

| Type 值 | 帧类型 |
|---------|--------|
| 0 | **管理帧** (Management) |
| 1 | 控制帧 (Control) |
| 2 | 数据帧 (Data) |
| 3 | 保留 |

**Beacon 帧识别：** Type=0, Subtype=8 → Frame Control = `0x0080`

```c
uint8_t frame_type = (hdr->frame_ctrl & 0x000C) >> 2;     // → 0
uint8_t frame_subtype = (hdr->frame_ctrl & 0x00F0) >> 4;  // → 8
```

---

### 2️⃣ Beacon 帧体固定部分 — 12 字节

```
payload[24~35]
┌──────────────────────┬──────────────────┬──────────────────┐
│       24~31         │      32~33       │      34~35       │
│     Timestamp       │ Beacon Interval  │  Capabilities    │
│     (8 bytes)       │    (2 bytes)     │   (2 bytes)      │
└──────────────────────┴──────────────────┴──────────────────┘
```

⚠️ **错误回顾：** 你之前代码写 `payload + 24`，把 Timestamp 当 Tag 解析了。Timestmap 是 8 字节随机数，所以输出了大量乱码。

Timestamp 之后的 **12 字节不是 Tag**，是 Beacon 帧特有的固定字段。真正的 Tag 从偏移 36 开始。

---

### 3️⃣ Tagged Parameters — 变长（从 payload[36] 开始）

这才是真正的 **IE (Information Element)** 列表：

```
payload[36] → 第一个 IE
┌──────┬──────┬─────────────────┬──────┬──────┬──────────┬──────┬──────┬──────...
│ IE 0 │ IE 0 │ IE 0 Value      │ IE 1 │ IE 1 │ IE 1 Value│ IE 3 │ IE 3 │ ...
│ ID   │ LEN  │ (SSID 字符串)    │ ID   │ LEN  │ (速率表)   │ ID   │ LEN  │
│ 0x00 │  N   │ ie[2]~ie[1+N]   │ 0x01 │  M   │ ...       │ 0x03 │  1   │
└──────┴──────┴─────────────────┴──────┴──────┴──────────┴──────┴──────┴──────...
```

#### 每个 IE 的通用格式

```
┌──────────┬──────────┬────────────────────────┐
│ ie[0]    │ ie[1]    │ ie[2] ...              │
│ Tag ID   │ Length   │ Value (Length 字节)     │
│ (1 字节)  │ (1 字节)  │                        │
└──────────┴──────────┴────────────────────────┘
←─────── 2 + Length 字节 ────────→
```

| 字段 | 位置 | 含义 |
|------|------|------|
| `ie[0]` | 当前 IE 的第 1 字节 | **Tag 编号**（SSID=0x00, 信道=0x03） |
| `ie[1]` | 当前 IE 的第 2 字节 | **Tag 长度**（值占多少字节） |
| `ie[2]` ~ `ie[1+tag_len]` | 后续字节 | **Tag 值** |

#### 常见的 Tag ID

| Tag ID | 名称 | 值的含义 |
|--------|------|---------|
| `0x00` | **SSID** | 网络名字符串 |
| `0x01` | Supported Rates | 支持的数据速率列表 |
| `0x03` | **DS Parameter Set** | 信道号（1 字节） |
| `0x05` | TIM | 流量指示图 |
| `0x07` | Country | 国家码 |
| `0x30` | RSN | WPA2 安全信息 |
| `0xDD` | Vendor Specific | 厂商自定义信息 |

---

## 三、完整解析流程

```c
// 1. MAC 头（24 字节）—— 通过结构体直接访问
ieee80211_frame_header_t *hdr = (ieee80211_frame_header_t *)pkt->payload;

// 2. 判断是不是 Beacon 帧
uint8_t frame_type = (hdr->frame_ctrl & 0x000C) >> 2;
uint8_t frame_subtype = (hdr->frame_ctrl & 0x00F0) >> 4;

if (frame_type == 0 && frame_subtype == 8)  // 管理帧 + Beacon
{
    // 3. 跳过 MAC 头(24) + 固定字段(12)，指向第一个 IE
    uint8_t *ie = pkt->payload + 36;
    int rest = pkt->rx_ctrl.sig_len - 36;

    while (rest >= 2)  // 每个 IE 至少 2 字节
    {
        uint8_t id = ie[0];      // Tag 编号
        uint8_t tag_len = ie[1]; // 值的长度

        if (id == 0)             // SSID
        {
            printf("SSID: ");
            for (int i = 0; i < tag_len; i++)
                printf("%c", ie[2 + i]);
            if (tag_len == 0) printf("(hidden)");
            printf("  ");
        }
        else if (id == 3)        // 信道
        {
            printf("CH: %d", ie[2]);
            printf("  ");
        }
        else if (id == 1)        // 支持速率
        {
            printf("Rates: ");
            for (int i = 0; i < tag_len; i++)
                printf("%d ", ie[2 + i] & 0x7F);
        }

        // 跳到下一个 IE
        ie += 2 + tag_len;
        rest -= 2 + tag_len;
    }
    printf("\n");
}
```

---

## 四、偏移量对照表

| 偏移 | 内容 | 长度 | 累计 |
|------|------|------|------|
| `0` | Frame Control + Duration | 4 | 4 |
| `4` | Address 1 (RA) | 6 | 10 |
| `10` | Address 2 (TA) | 6 | 16 |
| `16` | Address 3 (BSSID) | 6 | 22 |
| `22` | Sequence Control | 2 | **24** |
| `24` | Timestamp | 8 | 32 |
| `32` | Beacon Interval | 2 | 34 |
| `34` | Capabilities | 2 | **36** |
| `36` | **Tagged Parameters (IE 列表)** | 变长 | — |

---

## 五、常见问题

### Q：为什么之前用 `payload + 24` 是错的？

`payload + 24` 指向了 **Timestamp** 的开头。Timestamp 是 8 字节的随机值，被当成 IE 解析后：

- `ie[0]` = Timestamp 的前 2 字节 → 随机数，不是 0x00
- 几乎不会命中 SSID 的 `id == 0` 判断
- 偶尔命中也打印出乱码

### Q：`ie` 为什么叫 `ie`？

IEEE 802.11 协议文档中，Tagged Parameters 里的每一个 Tag 正式名称为 **Information Element**，缩写就是 **IE**。

### Q：怎么跳转到下一个 IE？

```c
ie += 2 + tag_len;   // 跳过当前 IE 的全部字节
rest -= 2 + tag_len; // 剩余字节数同步减少
```