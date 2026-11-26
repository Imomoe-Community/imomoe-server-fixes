<div align="center">

# Imomoe Server Fixes (`imomoe_server_fixes`)

[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](LICENSE)
[![C++: C++23](https://img.shields.io/badge/C%2B%2B-C%2B%2B23-orange.svg)](CMakeLists.txt)
[![CS2: Compatible](https://img.shields.io/badge/CS2-Dedicated%20Server-black.svg)](https://www.counter-strike.net/)
[![Platform: Plugify](https://img.shields.io/badge/Plugify-v3.1.0%2B-purple.svg)](https://github.com/untrustedmodders/plugify)
[![CI](https://github.com/Imomoe-Community/imomoe-server-fixes/actions/workflows/ci.yml/badge.svg)](https://github.com/Imomoe-Community/imomoe-server-fixes/actions/workflows/ci.yml)

**高性能、轻量级的 Counter-Strike 2 服务端底层缺陷修复与容量兼容扩展**<br>
<sub><sup>A high-performance, lightweight Counter-Strike 2 dedicated server fixes and capacity compatibility extension.</sup></sub>

**双语 / Bilingual** · [English](README.en.md)

</div>

---

### 项目简介 / Overview

**imomoe_server_fixes** 是面向 Counter-Strike 2 (CS2) 专用服务端的独立原生 C++ [Plugify](https://github.com/untrustedmodders/plugify) 插件。它专注于解决服务端层面的底层引擎缺陷、高危崩溃漏洞、聊天准入拦截、人质救援兼容、阵营出生点扩容以及旁观者容量同步等核心运行问题。
<sub><sup>**imomoe_server_fixes** is an independent native C++ [Plugify](https://github.com/untrustedmodders/plugify) plugin for Counter-Strike 2 dedicated servers. It focuses on resolving low-level engine defects, crash exploits, early chat admission, hostage rescue compatibility, spawn capacity auto-expansion, and spectator slot synchronization.</sup></sub>

本插件不依赖 `FunRounds` 或 `funrounds_native`，既可作为纯粹的服务端基础修复插件独立部署，也可以通过导出的所有权桥接接口与 `FunRounds` 协同运行。
<sub><sup>The plugin has zero runtime dependency on `FunRounds` or `funrounds_native`. It can run independently as an essential server maintenance suite, or cooperate seamlessly with bridge-aware `FunRounds` installations via exported capability interfaces.</sup></sub>

---

### 核心特性 / Key Features

| 维度 / <sub><sup>Category</sup></sub> | 机制与功能说明 / <sub><sup>Description</sup></sub> | 默认状态 / <sub><sup>Default</sup></sub> |
| :--- | :--- | :--- |
| **聊天准入管控**<br><sub><sup>Chat Admission</sup></sub> | 严格拦截未完全加载入服（未达到 `InGame` 状态）的真实客户端发送的 `say`、`say_team` 及 SDK 聊天别名指令；RCON、服务端命令、非聊天控制台指令及正常在局玩家不受影响。<br><sub><sup>Blocks `say`, `say_team`, and SDK chat aliases from clients that have not reached `InGame` status. Server/RCON commands and connected players are unaffected.</sup></sub> | 启用<br><sub><sup>`sf_chat_admission 1`</sup></sub> |
| **人质救援与背负**<br><sub><sup>Hostage Compatibility</sup></sub> | 1. **CT 救援判定补偿**：监控背负人质进入救援区的触碰堆栈，当抵达救援派发点但引擎未触发 `OnRescueZoneTouch` 时，在核验存活与实体句柄后自动补发一次输入，奖励与计分仍交由引擎处理。<br>2. **T 阵营人质拾取**：仅在 `mp_only_cts_rescue_hostages` 显式设为 `0` 时安全补丁目标选择器以允许 T 拾取人质；设为 `1` 或停用时完整恢复原厂机器码。<br><sub><sup>1. CT rescue compensation: safely resubmits missing `OnRescueZoneTouch` inputs when touches fail to dispatch. 2. T hostage pickup: conditionally patches hostage target selection when `mp_only_cts_rescue_hostages 0`.</sup></sub> | 启用<br><sub><sup>`sf_hostage_compat 1`</sup></sub> |
| **出生点容量扩容**<br><sub><sup>Spawn Capacity</sup></sub> | 在所有地图上自动补齐双方阵营的出生点，使 T 与 CT 均可容纳服务器 `maxplayers` 上限；因出生点不足加队失败时自动补齐并重试；同时提供服务端控制台手动点位增删与持久化保存工具。<br><sub><sup>Automatically fills spawn points for both teams up to `maxplayers` on all maps, retries team joins, and provides manual server console spawn editor commands.</sup></sub> | 启用<br><sub><sup>`sf_spawn_capacity 1`</sup></sub> |
| **观察者容量同步**<br><sub><sup>Spectator Capacity</sup></sub> | 动态将 `mp_spectators_max` 与 `CCSGameRules::m_iSpectatorSlotCount` 锚定至真实 `maxplayers`；若被手动修改或检测到外部冲突则平滑挂起同步，停用或换图时仅安全回退属于本插件的基准值。<br><sub><sup>Synchronizes `mp_spectators_max` and `CCSGameRules` spectator slots to `maxplayers` while safely releasing ownership on manual override, map change, or unload.</sup></sub> | 启用<br><sub><sup>`sf_spectator_compat 1`</sup></sub> |
| **原生底层缺陷修复**<br><sub><sup>Native Fault Fixes</sup></sub> | 默认启用光束坐标、过滤器无触发者和装备预缓存修复；Hammer ID、CSTV 配额筛选、水中跳跃、单 tick 起跳上限、敌人移动碰撞跟随队友规则和 KeyValues 限流按需启用。详见 [原生修复说明](docs/NATIVE_FAULTS.md)。<br><sub><sup>Beam, filter activator, and equipment precache fixes are enabled by default. Hammer ID, CSTV quota selection, water jumping, per-tick jump limits, enemy movement collision following teammate rules, and KeyValues rate limiting are opt-in.</sup></sub> | 基础项默认开启<br><sub><sup>Core fixes active</sup></sub> |
| **可选兼容与防护**<br><sub><sup>Opt-in Compatibility Guards</sup></sub> | 语音检查与限流、换队限流、关闭作弊后清除 noclip、Workshop 地图语音分流，以及观察者残留活体修复。<br><sub><sup>Voice validation and rate limits, team-switch rate limits, noclip reset when cheats are disabled, Workshop map speaker streams, and spectator pawn reconciliation.</sup></sub> | 可选开启<br><sub><sup>Opt-in (disabled by default)</sup></sub> |
| **地图运维与滑坡移动**<br><sub><sup>Map Maintenance & Ramp</sup></sub> | 1. **赛末换图恢复 (`sf_blank_map`)**：无下一张地图且引擎未安排换图时，重新加载当前地图。<br>2. **长图时钟刷新 (`sf_slow_animation`)**：按地图时长和玩家活动安排刷新。<br>3. **斜坡移动 (`sf_rampbug`)**：复查接缝处不一致的碰撞，保留落到缓坡时的水平速度。<br><sub><sup>1. Reloads the current map if a match ends without a queued destination. 2. Schedules long-running map refreshes based on elapsed time and player activity. 3. Retraces inconsistent ramp seams and preserves horizontal speed on slope landings.</sup></sub> | 可选开启<br><sub><sup>Opt-in (disabled by default)</sup></sub> |
| **游戏封禁残留修复**<br><sub><sup>Game Ban Fix</sup></sub> | 核对当前连接的 Steam 认证身份，消除 `STEAM_ID_PENDING` 误判与旧控制器残留引发的误踢；清理已过期的 GC 封禁并严格保留有效封禁。<br><sub><sup>Cleans expired GC bans and avoids pending SteamID kick cycles while strictly retaining active game bans.</sup></sub> | 可选开启<br><sub><sup>`sf_game_ban_fix 0`</sup></sub> |

<details>
<summary><b>点击展开查看原生引擎修复列表 / Click to expand Native Fault Fixes details</b></summary>

- **光束坐标卡死修复 (`sf_beam_fix`)**：修复无父级实体的光束/激光（`CBeam`）设置原点或端点坐标时沿用父级层级分发引起的引擎死锁与崩溃。<br><sub><sup>Prevents engine deadlocks when setting coordinates on unparented `CBeam` entities.</sup></sub>
- **过滤器空触发者修复 (`sf_filter_activator_fix`)**：拦截 `TestActivator` 在触发者指针为空时（如计时器/逻辑中继触发）的误判，避免异常触发 `OnPass`/`OnFail` 输出。<br><sub><sup>Suppresses improper filter pass/fail firing when `TestActivator` input lacks a valid activator.</sup></sub>
- **装备预缓存跳过 (`sf_equip_precache_fix`)**：在 `game_player_equip` 预缓存阶段跳过 `item_*` 物品名称查找，消除引擎告警并保留地图实际配发逻辑。<br><sub><sup>Bypasses erroneous `item_*` precache lookups during map load without altering gameplay equipment distribution.</sup></sub>
- **地图实体 Hammer ID 保留 (`sf_hammer_id`)**：在实体初始化阶段安全跳过 Hammer ID 抹除补丁，使地图运行时实体保留原始 `hammerUniqueId`。<br><sub><sup>Retains original `hammerUniqueId` values across entity spawn passes.</sup></sub>
- **CSTV 与 BOT 配额管理 (`sf_cstv_bot_quota`)**：在原生配额缩减遍历 BOT 时跳过 CSTV 客户端槽位，防止广播槽位被误计入踢出候选。<br><sub><sup>Exempts CSTV clients when balancing bot quota removals.</sup></sub>
- **水中单次跳跃判定 (`sf_water_jump`)**：跳过出水瞬间紧邻的一次触底分类，恢复顺畅连贯的出水跳跃手感。<br><sub><sup>Skips anomalous single-tick ground reclassification when exiting water bodies.</sup></sub>
- **单 Tick 起跳限流 (`sf_jump_spam`)**：每个实体在单个移动 Tick 仅结算一次真实起跳，杜绝连跳脚本单帧超频请求造成的物理异常。<br><sub><sup>Limits physical jump execution to once per tick per entity while preserving normal bunnyhop servers.</sup></sub>
- **KeyValues 指令洪水拦截 (`sf_keyvalues_flood`)**：对客户端网络流进行平滑令牌桶限流，丢弃超额数据包以防恶意攻击击垮引擎，不粗暴封禁客户端。<br><sub><sup>Applies connection-metered rate limiting against malformed KeyValues network packet spam.</sup></sub>

详见 [原生修复技术剖析文档 (docs/NATIVE_FAULTS.md)](docs/NATIVE_FAULTS.md)。
<sub><sup>See [Native Faults Documentation (docs/NATIVE_FAULTS.md)](docs/NATIVE_FAULTS.md) for full root-cause analyses and verification steps.</sup></sub>

</details>

<details>
<summary><b>点击展开底层架构与可靠性设计 / Click to expand Architecture & Reliability</b></summary>

- **二进制完整性与防冲突门禁 (`ServerCodeUnmodified`)**：挂载任何 PolyHook Detour 前直接校验磁盘文件（`server.dll`/`engine2.dll` 或 `.so`）指令字节。若目标区间存在未知篡改或已存在其他外部 Detour（如 CS2Fixes），插件安全放弃下钩（Fail-Closed）并在 `sf_status` 报告，杜绝双重 Hook 崩溃。<br><sub><sup>Verifies disk image instruction bytes before detouring. Automatically aborts on external hooks (e.g. CS2Fixes) to prevent double-hook crashes.</sup></sub>
- **内存补丁所有权 (`OwnedCodePatch`)**：T 阵营背人质与 Hammer ID 共用补丁写入和页保护处理。停用或卸载时只还原仍由本插件持有的字节。<br><sub><sup>Hostage pickup and Hammer ID share patch and page-protection handling. Only bytes still owned by this plugin are restored on disable or unload.</sup></sub>
- **跨模块多映像定位 (`LocateServerImage` / `LocateEngineImage`)**：支持安全定位游戏服务端主库与引擎底层库，提供页面只读与可执行边界检查，执行跨库消息限流与换图监听。<br><sub><sup>Resolves both server and engine2 libraries with memory boundary verification.</sup></sub>
- **FunRounds 所有权桥接**：导出 `GetOwnedFixes()` 固定返回位掩码 `3`（bit 0 对应 `chat_admission`，bit 1 对应 `hostage_compat`），与上层玩法插件解耦互通。<br><sub><sup>Exports fixed capability mask `3` so FunRounds delegates chat and hostage modules cleanly.</sup></sub>

</details>

---

### 安装与部署 / Installation

#### 前置环境要求 / Prerequisites

- **游戏服务端 / Dedicated Server**：Counter-Strike 2 Dedicated Server (Windows x64 / Linux x64)
- **Plugify 框架 / Plugify Host**：`>= 3.1.0`
- **基础 SDK 依赖 / S2SDK**：要求 S2SDK `>= 3.4.3`；[配对构建使用官方 3.5.1](docs/DEPENDENCIES.md)。<br><sub><sup>Requires S2SDK `>= 3.4.3`; the [paired build uses official 3.5.1](docs/DEPENDENCIES.md).</sup></sub>
- **底层 Hook 插件 / PolyHook**：最低 `>= 2.5.9`，双端配对验证使用官方 [`2.5.9`](https://github.com/untrustedmodders/plugify-plugin-polyhook/releases/tag/v2.5.9)。<br><sub><sup>Minimum `>= 2.5.9`; Windows and Linux were validated with the official `2.5.9` release.</sup></sub>

#### 部署结构 / Deployment Structure

将发布包文件部署至游戏服务端根目录：
<sub><sup>Deploy plugin artifacts to the dedicated server root directory:</sup></sub>

```text
game/csgo/
└── addons/plugify/envs/imomoe_server_fixes/
    ├── bin/
    │   └── imomoe-server-fixes.dll                 # Linux: libimomoe-server-fixes.so
    ├── gamedata/
    │   └── imomoe_server_fixes.jsonc               # 动态特征码与偏移 / Gamedata signatures
    └── imomoe_server_fixes.pplugin                 # 插件描述清单 / Manifest
```

插件在启动和切换到另一张地图时加载 `game/csgo/cfg/imomoe/imomoe_server_fixes.cfg`；同图 `changelevel` 地图重载不重复执行（插件卸载重加载会重新执行）。文件缺失时生成默认配置，已有内容保留；不用在 `server.cfg` 再执行。<br><sub><sup>The plugin loads this file at startup and when switching to a different map. Reloading the same map does not run it again. Defaults are generated only if the file is missing; no `server.cfg` entry is needed.</sup></sub>

**从旧路径升级：**首次启动新版前，将要保留的设置写入新路径，保留旧 `cfg/imomoe_server_fixes.cfg` 作备份，并移除 `server.cfg`、启动参数或运维脚本里的旧 `exec` 调用。插件不读取或移动旧文件。<br><sub><sup>Before starting the upgraded plugin, transfer your settings to the new path, keep the original as a backup, and remove the old `exec` from server configuration and launch scripts. The plugin leaves the legacy file untouched.</sup></sub>

---

### 指令列表 / Commands Reference

| 控制台指令 <br><sub><sup>Command</sup></sub> | 参数 <br><sub><sup>Arguments</sup></sub> | 说明 <br><sub><sup>Description</sup></sub> |
| :--- | :--- | :--- |
| `sf_status` | 无 <br><sub><sup>None</sup></sub> | 检查所有模块的启用状态（`enabled`）、运行期可用性（`available`）以及各 Hook/Patch 的具体诊断信息。<br><sub><sup>Displays status, runtime availability, and hook/patch diagnostics for all modules.</sup></sub> |
| `sf_spawn_add` | `<T\|CT> <x y z> [pitch yaw roll]` | 在指定世界坐标及朝向手动为 T 或 CT 队伍添加出生点（立即生效）。<br><sub><sup>Adds a manual spawn point for T or CT at specified world coordinates.</sup></sub> |
| `sf_spawn_list` | 无 <br><sub><sup>None</sup></sub> | 列出当前地图上所有已手动添加的出生点 ID 与坐标。<br><sub><sup>Lists all active manual spawn points for the current map.</sup></sub> |
| `sf_spawn_remove` | `<id>` | 移除指定 ID 的手动出生点（不影响地图原生出生点）。<br><sub><sup>Removes a specific manual spawn point without touching native spawns.</sup></sub> |
| `sf_spawn_save` | 无 <br><sub><sup>None</sup></sub> | 将当前地图的手动出生点保存至 `cfg/imomoe_server_fixes/spawn_capacity/<map>.cfg`，下次加载该图时恢复。<br><sub><sup>Saves manual spawn points for the next time the map loads.</sup></sub> |

`hostage_compat` 的 `CT` 显示当前救援区观察器的状态；地图没有可用的救援区或游戏类型不支持时会显示原因。`T` 独立判断，因此 `available=1` 也可能只表示 T 侧可用。`CT=ready` 表示观察器就绪，单次救援结果需查看原生事件。<br><sub><sup>`CT` reports rescue-zone observer readiness and its map or game-mode restriction. `T` is independent, so `available=1` may reflect only T-side availability. `CT=ready` describes the observer; use native events to check an individual rescue.</sup></sub>

---

### 配置说明 / Configuration

默认值与完整选项见 [配置模板](configs/imomoe_server_fixes.cfg)。在 `game/csgo/cfg/imomoe/imomoe_server_fixes.cfg` 中按需调整；换图后用 `sf_status` 查看结果，也可在服务端控制台手动执行 `exec imomoe/imomoe_server_fixes.cfg`。<br><sub><sup>Edit the generated file and check `sf_status` after the next map load. You can also run the `exec` command above from the server console.</sup></sub>

| 用途 / <sub><sup>Use case</sup></sub> | 设置 / <sub><sup>Setting</sup></sub> |
| :--- | :--- |
| 只检查语音是否包含音频字段 / <sub><sup>Check that voice messages contain audio</sup></sub> | `sf_voice_fix 1` |
| 对每个客户端限制语音消息速率 / <sub><sup>Rate-limit voice messages per client</sup></sub> | `sf_voice_max_per_second 128` |
| 限制每客户端 10 秒内的换队与观战指令 / <sub><sup>Limit team and spectator switches per client over 10 seconds</sup></sub> | `sf_team_switch_limit 15` |
| 关闭 `sv_cheats` 后恢复走路，重生不继承 noclip / <sub><sup>Restore walking when cheats are disabled; clear retained noclip across respawns</sup></sub> | `sf_noclip_reset 1` |
| 分离 Workshop 地图上的说话者语音流 / <sub><sup>Separate speaker streams on Workshop maps</sup></sub> | `sf_workshop_voice 1` |

`sf_noclip_reset` 仅保留可选的 `imomoe_admin`（≥ 0.1.0）明确认领的当前玩家槽位和 pawn 句柄对应的 noclip。换 pawn 不继承豁免；未安装 Admin 时仍正常清理。<br><sub><sup>`sf_noclip_reset` preserves noclip only for the exact current slot and pawn handle claimed by optional `imomoe_admin` (≥ 0.1.0). The exemption does not carry over to a new pawn; cleanup works without Admin.</sup></sub>

限额为 `0` 时关闭对应限流；超过限额的消息会丢弃，不会踢出客户端。地图维护与移动选项见 [原生修复说明](docs/NATIVE_FAULTS.md)。<br><sub><sup>A limit of `0` disables that rate guard. Excess messages are dropped without disconnecting the client. See [Native Fault Fixes](docs/NATIVE_FAULTS.md) for map maintenance and movement settings.</sup></sub>

<details>
<summary><b>配置迁移说明 / Configuration Migration</b></summary>

若此前在 `FunRounds` 的 TOML 配置文件中使用过 `hostage_rescue_compat`，可使用内置迁移工具生成对应的 `.cfg`：
<sub><sup>Migrate legacy FunRounds TOML settings to `.cfg`:</sup></sub>

```powershell
python tools/migrate_config.py <path-to-funrounds.toml> --output hostage-setting.cfg
```

将生成的设置写入新路径的配置文件，保留其中其他内容。<br><sub><sup>Transfer the generated setting to the new CFG without replacing its other contents.</sup></sub>

</details>

---

### 构建与验证 / Building & Testing

#### 1. 策略与单元测试（无需 SDK 运行时） / Policy & Unit Tests

策略测试覆盖全部 8 项核心策略（`chat_policy`、`chat_listener`、`hostage_policy`、`spawn_capacity_policy`、`spectator_compat_policy`、`compat_guard_policy`、`map_maintenance_policy`、`ramp_policy`），在纯净环境下无需依赖引擎运行时即可编译与执行：
<sub><sup>Tests 8 independent policy suites without requiring engine runtimes:</sup></sub>

```sh
# 编译并运行 8 项核心策略测试
cmake --preset policy
cmake --build --preset policy --parallel
ctest --preset policy

# 运行 Python 配置迁移与构建测试
python -m unittest discover -s tests -p "test_*.py"
python -m unittest discover -s tools/tests -p "test_*.py"
```

<a id="build-the-plugin-with-the-paired-sdk"></a>
#### 2. 使用配对 SDK 编译完整插件 / Full Plugin Build

生产构建需要 C++23 编译器，并基于 [依赖输入清单 (docs/DEPENDENCIES.md)](docs/DEPENDENCIES.md) 指定的配对 S2SDK 工作树：
<sub><sup>Production builds require C++23 and the paired S2SDK worktree specified in [DEPENDENCIES.md](docs/DEPENDENCIES.md):</sup></sub>

```powershell
# Windows 构建示例 (PowerShell)
$s2sdkSource = '<path-to-paired-s2sdk-source>'
$s2sdkBuild = '<path-to-paired-s2sdk-build>'
cmake -S . -B build `
  "-DSERVER_FIXES_PLUGIFY_INCLUDE_DIR=$s2sdkSource/external/plugify/include" `
  "-DSERVER_FIXES_S2SDK_GENERATED_INCLUDE_DIR=$PWD/generated-s2sdk/external/plugify/include" `
  "-DSERVER_FIXES_SOURCE_SDK_PUBLIC_INCLUDE_DIR=$s2sdkSource/external/sourcesdk/public" `
  "-DSERVER_FIXES_SOURCE_SDK_BUILD_DIR=$s2sdkBuild" `
  "-DSERVER_FIXES_SOURCE_SDK_LIBRARY_DIR=$s2sdkSource/external/sourcesdk/lib/win64" `
  "-DSERVER_FIXES_SOURCE_SDK_PROTO_INCLUDE_DIR=$s2sdkBuild/external/sourcesdk/protos"
cmake --build build --config Release
```

#### 3. 自动化发布打包 / Paired Release Packaging

使用官方 S2SDK 的源码和配对构建。Windows 使用 Visual Studio x64 生成器构建 S2SDK；Linux 在 Steam Runtime GCC 14 容器内以相同绝对路径构建，再将该镜像传给打包脚本。<br><sub><sup>Pass the official S2SDK source and its paired build. On Windows, use a Visual Studio x64 generator to build S2SDK. On Linux, build S2SDK at the same absolute paths inside a Steam Runtime GCC 14 container and pass that image to the packager.</sup></sub>

```powershell
# Windows
python tools/build_paired.py --sdk-source <s2sdk-source> --sdk-build <s2sdk-build> --cs2-bin <game/bin/win64>

# Linux (带 Docker 环境)
python3 tools/build_paired.py --sdk-source <s2sdk-source> --sdk-build <s2sdk-build> --steamrt-image <image> --cs2-bin <game/bin/linuxsteamrt64>
```

产物将输出至 `dist/imomoe-server-fixes-<platform>-<name>.zip`。
<sub><sup>Artifacts generated into `dist/imomoe-server-fixes-<platform>-<name>.zip`.</sup></sub>

设置 `IMOMOE_WORK_ROOT` 可将构建会话移至该目录的 `sessions/serverfixes-<name>`。使用 `--funrounds-source` 时，需选用支持共享工作目录的 FunRounds 构建脚本；SDK 会话位于 `sessions/serverfixes-sdk-<name>`。变量未设置或仅含空白时，两个会话分别保留在 ServerFixes 与 FunRounds 各自的 `.work/` 中。<br>
<sub><sup>Set `IMOMOE_WORK_ROOT` to put the build session in its `sessions/serverfixes-<name>` directory. With `--funrounds-source`, use a FunRounds SDK builder that supports shared work roots; its session goes in `sessions/serverfixes-sdk-<name>`. If the variable is unset or blank, the sessions stay in the respective ServerFixes and FunRounds `.work/` directories.</sup></sub>

- [依赖与构建输入 / Dependencies](docs/DEPENDENCIES.md) · [原生修复说明 / Native Faults](docs/NATIVE_FAULTS.md) · [贡献指南 / Contributing](CONTRIBUTING.md) · [安全通告 / Security](SECURITY.md) · [版本记录 / Changelog](CHANGELOG.md)

---

### 开源协议与作者 / License & Author

- **作者 / Author**：Mai Ooizumi ([@Mai-Ooizumi](https://github.com/Mai-Ooizumi))
- **参考项目 / Reference**：[ViTaminza/plugify-game_fixes](https://github.com/ViTaminza/plugify-game_fixes)（兼容与防护、原生修复的行为参考 / behavior reference for compatibility guards and native fixes）
- **开源协议 / License**：GNU Affero General Public License v3.0 or later ([AGPL-3.0-or-later](LICENSE))
