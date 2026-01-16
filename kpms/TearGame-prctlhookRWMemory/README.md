# 法律声明与免责条款

## Legal Notice & Disclaimer

---

## 开源声明

本项目 **TearGame KPM** 是一个完全开源、免费、公开的技术研究项目。

- **开源协议**: GPL-2.0-or-later
- **收费情况**: 完全免费，无任何收费
- **商业用途**: 禁止用于任何商业目的

**如果有人向你收费出售本项目，那是骗子！**

---

## 作者信息

**原作者**: 阿夜 (AYssu)
- GitHub: https://github.com/AYssu
- QQ: 2997036064
- QQ群: 903485530

**二次开发**: 泪心 (TearHacker)
- GitHub: https://github.com/tearhacker
- Telegram: https://t.me/TearGame
- QQ: 2254013571

---

## 免责声明

### 1. 仅供学习研究

本项目仅供以下目的使用：
- 学习 Linux 内核模块开发
- 研究 Android 系统底层机制
- 了解跨进程内存访问技术
- 安全研究与漏洞分析

### 2. 禁止非法用途

严禁将本项目用于：
- ❌ 游戏作弊、外挂开发
- ❌ 破解付费软件
- ❌ 窃取用户隐私数据
- ❌ 任何违反法律法规的行为
- ❌ 任何侵犯他人权益的行为

### 3. 使用者责任

- 使用本项目即表示您已阅读并同意本声明
- 使用者需自行承担使用本项目产生的一切后果
- 作者不对任何直接或间接损失负责
- 使用者需遵守所在国家/地区的法律法规

### 4. 无担保声明

本项目按"原样"提供，不提供任何形式的担保，包括但不限于：
- 适销性担保
- 特定用途适用性担保
- 非侵权担保

---

## 版权声明

```
Copyright (C) 2023-2025 AYssu, TearHacker

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
```

---

## 举报渠道

如发现有人利用本项目从事非法活动或收费出售，请联系：
- Telegram: https://t.me/TearGame
- QQ: 2254013571

---

**请合法、合规、负责任地使用本项目。**

**Please use this project legally, compliantly, and responsibly.**

---

## 技术说明 - KPM 内核模块功能范围

### 模块概述

TearGame KPM 是基于 KernelPatch 框架开发的内核模块，通过 Hook `prctl` 系统调用实现跨进程内存读写功能。

### 支持的功能

| 命令码 | 功能 | 说明 |
|--------|------|------|
| `0x4D454D01` | 内存读取 | 使用 `access_process_vm` 读取目标进程内存 |
| `0x4D454D02` | 内存写入 | 使用 `access_process_vm` 写入目标进程内存 |
| `0x4D454D03` | 获取 PID | 通过短包名（≤15字符）获取进程 PID |
| `0x4D454D04` | 安全读取 | 物理内存方式读取（开发中） |
| `0x4D454D05` | 安全写入 | 物理内存方式写入（开发中） |

### 内核符号依赖

模块通过 kallsyms 动态解析以下内核符号：

```
__arch_copy_from_user  - 用户空间数据拷贝
find_task_by_vpid      - 通过 PID 查找 task_struct
access_process_vm      - 跨进程内存访问（核心功能）
memremap / memunmap    - 物理内存映射（备用方案）
```

### 已知限制

1. **PID 获取限制**
   - `task_struct->comm` 字段最大 15 字符
   - 长包名（如 `com.tencent.tmgp.sgame`）会被截断
   - 建议使用用户态 `/proc/<pid>/cmdline` 获取完整包名

2. **模块基址获取**
   - Linux 6.1+ 使用 Maple Tree 管理 VMA
   - 内核层遍历 VMA 不可行
   - 必须使用用户态读取 `/proc/<pid>/maps`

3. **内存写入限制**
   - 只读内存区域（如代码段）写入会失败
   - 这是 `access_process_vm` 的正常行为

### 兼容性

- **内核版本**: Linux 4.x / 5.x / 6.x
- **架构**: ARM64 (aarch64)
- **框架**: KernelPatch 0.10.x+
- **测试设备**: Realme GT8 Pro (kernel 6.1.x)


