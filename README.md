# Imomoe Server Fixes (`imomoe_server_fixes`)

[简体中文](#简体中文) | [English](#english)

---

## 简体中文

`imomoe_server_fixes` 是一个面向 Counter-Strike 2 (CS2) 专用服务器的独立 C++ [Plugify](https://github.com/untrustedmodders/plugify) 插件，专门负责处理服务端层面的底层修复与容量兼容问题。它不依赖 `FunRounds` 或 `funrounds_native`，既可以单独部署，也可以与支持桥接的 `FunRounds` 配合运行。

### 功能概览

插件包含四个可独立开关的模块：

| 模块标识 | 控制 ConVar | 默认值 | 功能说明 |
| --- | --- | --- | --- |
| `chat_admission` | `sf_chat_admission` | `1` | **聊天准入拦截**：拦截尚未完全进入游戏（未达到 `InGame` 状态）的真实客户端槽位发送的 `say`、`say_team` 及 SDK 聊天别名指令；RCON、服务端命令、非聊天命令和已正常进服的玩家不受影响。触发拦截时仅在日志记录槽位号与规范命令名。 |
| `hostage_compat` | `sf_hostage_compat` | `1` | **人质救援与背负兼容**：<br>1. **CT 救援触发补偿**：当存活的 CT 背负人质触碰救援区（`CBaseEntity::Touch`）且已到达 pawn 救援派发点，但原生 `CHostage_API::OnRescueZoneTouch` 未被触发时，在核验地图、回合、存活状态、救援区及实体句柄后，自动补发一次原生 `OnRescueZoneTouch` 输入。资金奖励、事件广播与回合结算仍由引擎原生逻辑负责。<br>2. **T 阵营背起人质**：仅当 `mp_only_cts_rescue_hostages` 明确为 `0` (`false`) 时，修改原生人质交互目标选择器的阵营判断以允许 T 背起人质；为 `1` 或未知时保持原生仅限 CT 的行为。关闭模块时会安全还原补丁，且不会打断 T 正在背负的人质。 |
| `spawn_capacity` | `sf_spawn_capacity` | `1` | **出生点容量扩展**：所有地图都会自动补齐 T、CT 双方的出生点，使每队可容纳服务器的 `maxplayers` 人；较低的原生队伍人数上限也会相应提高。玩家因出生点不足而加队失败时，补点后会重试一次。支持手动添加和保存出生点。 |
| `spectator_compat` | `sf_spectator_compat` | `1` | **观察者容量同步**：自动将 `mp_spectators_max` 与 `CCSGameRules::m_iSpectatorSlotCount` 对齐至服务器实际 `GetMaxClients()`（`maxplayers`）。若运行期间有其他插件或管理员主动修改了某项数值，本模块会在当前启用周期内自动让出该属性的控制权，并在禁用、换图或卸载时仅还原仍由本插件持有的数值。 |

#### 与 FunRounds 的职责桥接

插件导出了 `GetOwnedFixes()` 接口并固定返回掩码 `3`（bit 0 代表 `chat_admission`，bit 1 代表 `hostage_compat`），用于通知支持所有权桥接的 `FunRounds` 由本插件接管这两项功能（返回值不随模块开关或运行期可用性变化）。`spawn_capacity` 与 `spectator_compat` 则是本插件新增的独立能力，不占用 `FunRounds` 掩码位。

> **注意**：与 `FunRounds` 混用时，请确保所安装的 `FunRounds` 构建包含 `GetOwnedFixes()` 桥接逻辑。旧版不含桥接的 `0.6.0` 二进制不可与本插件混装（仅凭版本号相同不足以判断兼容性）。

---

### 安装与配置

1. 将插件包部署至 Plugify 插件目录，并确保运行环境所使用的 S2SDK 插件清单（`.pplugin`）声明了 `OnMapStart_Register` 与 `OnMapStart_Unregister`（可直接采用本仓库提供的 [`bindings/s2sdk.pplugin`](bindings/s2sdk.pplugin)）。
2. 将 [`configs/imomoe_server_fixes.cfg`](configs/imomoe_server_fixes.cfg) 复制到游戏目录的 `game/csgo/cfg/imomoe_server_fixes.cfg`：
   ```cfg
   sf_chat_admission 1
   sf_hostage_compat 1
   sf_spawn_capacity 1
   sf_spectator_compat 1
   ```
3. 在 `game/csgo/cfg/server.cfg` 中添加加载项：
   ```cfg
   exec imomoe_server_fixes.cfg
   ```

#### 从旧版 FunRounds 迁移配置

如果你此前在 `FunRounds` 的 TOML 配置文件中使用过 `hostage_rescue_compat`，可以使用自带的迁移脚本生成对应的 `.cfg` 文件（显式的 `false` 或缺省未配置均会映射为 `sf_hostage_compat 0`）：

```powershell
python tools/migrate_config.py <path-to-funrounds.toml> --output configs/imomoe_server_fixes.cfg
```

为防止误覆盖，目标文件已存在时需额外传入 `--overwrite`。

---

### 控制台命令

#### 1. 查看模块运行状态

在控制台执行 `sf_status` 可实时查看四个模块的启用状态（`enabled`）、运行期可用性（`available`）及具体原因（`reason`）：

```text
sf_status
```

> 对于 `hostage_compat`，`sf_status` 会在 `reason` 中分别报告 CT 救援补偿路径与 T 背负补丁路径的可用性；只要其中任意一条路径可用，`available` 即为 `1`。

#### 自动扩容的工作方式

默认配置会在所有地图上自动检查并补齐双方容量。用 `sf_spawn_capacity 0` 关闭出生点模块；重新设为 `1` 后会按当前地图和人数上限重建。关闭、换图或卸载时，插件清理自己创建的出生点，并恢复仍由自己控制的队伍上限。其他插件或管理员改过的上限会保留，`sf_status` 会显示具体原因。

自动补点会复用已有同阵营出生点的位置，因此更多人可以加入队伍，但出生位置仍可能重叠。需要分散玩家时，用下面的手动工具添加不同位置。`sf_status` 中的 `available=1` 表示双方容量均已补齐。某队没有可用模板时会显示 `available=0`，并提示用 `sf_spawn_add` 为该队补点；另一队和已加载的手动出生点仍可使用。服务器人数上限变化、每回合开始和手动增删点后都会重新计算容量。地图或模式自身的队伍平衡规则仍然生效。

#### 2. 手动管理地图出生点（仅限服务端控制台）

手动出生点命令必须在**服务端控制台**中执行。新添加的出生点会立即在当前地图内存中生效，甚至可用于挽救缺少原生有效出生点的阵营；执行 `sf_spawn_save` 后会持久化保存至游戏目录下的 `cfg/imomoe_server_fixes/spawn_capacity/<map>.cfg`，并在下次地图启动、首个正式回合开始前自动重建。`sf_spawn_remove` 仅允许删除通过该工具创建的手动出生点，绝不会触碰地图原生出生点。

```text
sf_spawn_add T 128 -256 32 [pitch yaw roll]
sf_spawn_add CT 128 -256 32 [pitch yaw roll]
sf_spawn_list
sf_spawn_remove <manual-point-id>
sf_spawn_save
```

---

### 构建与测试

#### 1. 快速运行策略与迁移测试（无需 SDK）

需要 CMake 3.24+、支持 C++20 的编译器以及 Python 3.11+。在 Windows 上请使用已配置编译器的开发者终端，或通过 `-G` 指定本地安装的生成器：

```sh
cmake --preset policy
cmake --build --preset policy --parallel 2
ctest --preset policy
python -m unittest discover -s tests -p "test_*.py"
```

`policy` 预设会将 Windows 与 Linux 的构建产物隔离在不同目录中。若需配置本地特定的生成器或 SDK 路径，请写入本地的 `CMakeUserPresets.json`（已被 Git 忽略）。

#### 2. 使用配对 SDK 构建完整插件

编译完整插件需要支持 C++23 的编译器，并使用 [依赖说明 (docs/DEPENDENCIES.md)](docs/DEPENDENCIES.md) 中固定的依赖版本与配对构建目录：

- **绑定与清单契约**：请使用配对 S2SDK 的 Plugify 头文件以及本仓库检入的 `generated-s2sdk/` 导出头。`bindings/s2sdk.pplugin` 是在 159 方法 / 6 原型清单的基础上，额外声明了 S2SDK 3.1.1 已实现的 `OnMapStart_Register` 与 `OnMapStart_Unregister`（共 161 方法 / 6 原型）。请勿使用 S2SDK 仓库根目录的 `plugify-plugin-s2sdk.pplugin.in` 模板重新生成。
- **配对 SourceSDK 与 `tier0` 链接**：加队重试逻辑使用了 S2SDK 固定子模块（`f3de33c58595c628c12b4ffaba76dfcd49b8763f`）中的强类型 `IServerGameClients::ClientCommand` 与 `CCommand`。构建时需提供同一 S2SDK 工作树下的 `public` 头文件目录与已构建的 `build` 目录（包含 `<build>/external/sourcesdk/protos` 生成的 Protobuf 头文件以及提供 `CCommand` 实现的静态库 `tier0_perproject`）。
  - **Windows** 链接 `build/external/sourcesdk/Release/tier0_perproject.lib` 与 `source/external/sourcesdk/lib/win64/tier0.lib`（运行时由 CS2 从 `game/bin/win64/tier0.dll` 提供）。
  - **Steam Runtime Linux** 使用 Sniper GCC 14 工具链，链接 `build/external/sourcesdk/libtier0_perproject.a` 与 `source/external/sourcesdk/lib/linuxsteamrt64/libtier0.so`（运行时由 CS2 从 `game/bin/linuxsteamrt64/libtier0.so` 提供）。

**Windows (PowerShell) 构建示例：**

```powershell
$pairedS2sdkWorktree = "<paired-pinned-s2sdk-build-worktree>"
$pairedS2sdk = Join-Path $pairedS2sdkWorktree "source"
$plugifyInclude = Join-Path $pairedS2sdk "external/plugify/include"
$generatedS2sdkInclude = Join-Path (Get-Location) "generated-s2sdk/external/plugify/include"
$sourceSdk = Join-Path $pairedS2sdk "external/sourcesdk"
$sourceSdkPublicInclude = Join-Path $sourceSdk "public"
$sourceSdkLibrary = Join-Path $sourceSdk "lib/win64"
$sourceSdkBuild = Join-Path $pairedS2sdkWorktree "build"
$sourceSdkProtoInclude = Join-Path $pairedS2sdkWorktree "build/external/sourcesdk/protos"
$tier0ImportLibrary = Join-Path $sourceSdkLibrary "tier0.lib"
$tier0PerprojectLibrary = Join-Path $sourceSdkBuild "external/sourcesdk/Release/tier0_perproject.lib"

if (-not (Test-Path -LiteralPath $tier0ImportLibrary)) { throw "Missing fixed SourceSDK import library: $tier0ImportLibrary" }
if (-not (Test-Path -LiteralPath $tier0PerprojectLibrary)) { throw "Missing paired SourceSDK static CCommand library: $tier0PerprojectLibrary" }
if (-not (Test-Path -LiteralPath (Join-Path $sourceSdkProtoInclude "netmessages.pb.h"))) { throw "Missing paired SourceSDK protocol headers: $sourceSdkProtoInclude" }

# 如需重新生成绑定，仅可基于本仓库的 bindings/s2sdk.pplugin 执行：
# plugify-gen -lang cpp -manifest (Join-Path (Get-Location) "bindings/s2sdk.pplugin") -output generated-s2sdk

cmake -S . -B build `
  "-DSERVER_FIXES_PLUGIFY_INCLUDE_DIR=$plugifyInclude" `
  "-DSERVER_FIXES_S2SDK_GENERATED_INCLUDE_DIR=$generatedS2sdkInclude" `
  "-DSERVER_FIXES_SOURCE_SDK_PUBLIC_INCLUDE_DIR=$sourceSdkPublicInclude" `
  "-DSERVER_FIXES_SOURCE_SDK_BUILD_DIR=$sourceSdkBuild" `
  "-DSERVER_FIXES_SOURCE_SDK_LIBRARY_DIR=$sourceSdkLibrary" `
  "-DSERVER_FIXES_SOURCE_SDK_PROTO_INCLUDE_DIR=$sourceSdkProtoInclude"
cmake --build build --config Release
```

---

### 版本兼容与技术细节

- **目标 CS2 服务端版本**：当前二进制特征码、偏移和人质阵营补丁绑定至 **Build 26393695 / ServerVersion 2000940 / PatchVersion 1.41.9.5**：
  - Windows `server.dll`：Timestamp `0x6AE91E94` / Image Size `0x257F000`
  - Linux `libserver.so`：GNU Build ID `b9eac3d151886d4fc820d2e1d1f1d8642daa54c9`
  - 当服务端二进制与上述标识不匹配时，依赖特征码或内存补丁的路径会自动安全禁用（Fail-closed），并通过 `sf_status` 报告原因。
- **CT 救援检测机制**：通过观察 `CBaseEntity::Touch`、派发器直接调用 pawn 救援函数前一条传参 `MOV` 指令上的寄存器快照 Mid-Hook（Windows 为 `mov rcx,rdi`，Linux 为 `mov rdi,rbx`，之所以锚定在 `MOV` 是因为 PolyHook 对 `CALL` 指令下钩子时会跟随进入被调函数），以及 `CHostage_API::OnRescueZoneTouch` 入口来判断救援是否被引擎正常处理。挂载前会校验指令字节、相邻直接调用、返回路径及被调函数序言；若存在外部 Detour 或无法明确分类的调用，则放弃当次补偿以避免误触发。
- **出生点生命周期与重算**：自动补充的出生点要求模板源同时通过原生几何空间校验并位于引擎主出生点列表中；运行期增删出生点会调用原生 `RegisterOrRecountSpawnPoints(nullptr)` 路径重算有效容量（仅校验完整句柄与启用状态，不会因为当前位置站有存活玩家而误判失效）。在换图、禁用或卸载时，插件会先禁用自建出生点并触发重算，再交由 S2SDK 请求引擎安全移除。
- **观察者上限换图恢复**：在地图结束（`OnMapEnd`）且原生 ConVar 仍处于安全可写阶段时，若 `mp_spectators_max` 仍由本插件持有，插件会先将其恢复为接管前的原始值，从而确保下一张地图的 `CCSGameRules` 实例从原始基准值初始化，而无需访问正在销毁的旧 GameRules 对象。
- **已有版本验证记录（全地图扩容修改前）**：Windows 与 Steam Runtime Linux 均已通过完整构建、策略/回调测试、最小插件验证及真机人质救援/C4 控制验收；Windows 已完成双客户端聊天拦截、满员 64 人（1 真人 + 63 BOT）容量及观察者死亡/掉落/视角切换/返回全流程验收。Linux 运行环境需配合 [PolyHook 页边界修复](docs/DEPENDENCIES.md#linux-polyhook-range-repair) 使用。

### 开源协议

本项目源码采用 **AGPL-3.0-or-later** 协议，详见 [`LICENSE`](LICENSE)。S2SDK 绑定等第三方文件保留各自的许可声明。

---

## English

`imomoe_server_fixes` is a standalone C++ [Plugify](https://github.com/untrustedmodders/plugify) plugin for Counter-Strike 2 (CS2) dedicated servers. It owns server-level fixes and capacity compatibility without importing `FunRounds` or `funrounds_native`, and can run either on its own or alongside a bridge-aware `FunRounds` installation.

### What It Owns

The plugin provides four independently toggleable modules:

| Module | ConVar | Default | Description |
| --- | --- | --- | --- |
| `chat_admission` | `sf_chat_admission` | `1` | **Chat Admission**: Blocks `say`, `say_team`, and the SDK Chat alias path for real client slots that are not yet `InGame`. Server/RCON commands, non-chat commands, and fully connected players pass through unchanged. Logs record only the slot number and canonical command label. |
| `hostage_compat` | `sf_hostage_compat` | `1` | **Hostage Compatibility**:<br>1. **CT Rescue Compensation**: Observes `CBaseEntity::Touch`, a build-guarded mid-hook immediately before the dispatcher's direct pawn-rescue call, and the native `CHostage_API::OnRescueZoneTouch` target-input entry. If the pawn rescue call site is reached with certain observation and `OnRescueZoneTouch` was not entered for the carried hostage on that Touch stack, Touch Post revalidates the map, round, player life, rescue zone, carried hostage, and prop before submitting a single native `OnRescueZoneTouch` input. Rewards, events, counters, cleanup, and round outcomes remain handled by the engine.<br>2. **T Hostage Pickup**: Patches the native hostage target selector's team check only when `mp_only_cts_rescue_hostages` is known to be `false`; `true` or unknown values keep the native CT-only check. Disabling the module restores the original selector bytes without interrupting a hostage already carried by a Terrorist. |
| `spawn_capacity` | `sf_spawn_capacity` | `1` | **Spawn Capacity**: Automatically fills T and CT spawn capacity on every map so either team can hold `maxplayers` players. Raises lower native team limits as needed. Retries a join request once after adding capacity for a spawn-related rejection. Includes tools to add and save spawn points manually. |
| `spectator_compat` | `sf_spectator_compat` | `1` | **Spectator Capacity**: Keeps `mp_spectators_max` and `CCSGameRules::m_iSpectatorSlotCount` aligned with `GetMaxClients()` (`maxplayers`). If an external writer modifies either value during an enabled cycle, the module yields that property for the rest of the cycle and restores only values it still owns on disable, map end, or unload. |

#### FunRounds Ownership Bridge

`GetOwnedFixes()` always returns `3`: bit 0 is `chat_admission` and bit 1 is `hostage_compat`. This mask stays constant regardless of whether individual modules are enabled or available at runtime. `spawn_capacity` and `spectator_compat` are standalone capabilities and do not add FunRounds ownership bits.

> **Important**: When running alongside `FunRounds`, pair this plugin only with a `FunRounds` build that includes the `GetOwnedFixes()` ownership bridge. Older `0.6.0` binaries without the bridge are not compatible, even if the version string matches.

---

### Configure and Check Status

1. Install the plugin and ensure the runtime S2SDK provider descriptor declares `OnMapStart_Register` and `OnMapStart_Unregister` (use this repository's [`bindings/s2sdk.pplugin`](bindings/s2sdk.pplugin) for the standalone installation).
2. Copy [`configs/imomoe_server_fixes.cfg`](configs/imomoe_server_fixes.cfg) to `game/csgo/cfg/imomoe_server_fixes.cfg`:
   ```cfg
   sf_chat_admission 1
   sf_hostage_compat 1
   sf_spawn_capacity 1
   sf_spectator_compat 1
   ```
3. Add the following line to `game/csgo/cfg/server.cfg`:
   ```cfg
   exec imomoe_server_fixes.cfg
   ```

#### Migrate the Legacy Hostage Setting

To migrate `hostage_rescue_compat` from an existing `FunRounds` TOML file into a `ServerFixes` CFG file (preserving explicit `false` and mapping an omitted key to `sf_hostage_compat 0`):

```powershell
python tools/migrate_config.py <path-to-funrounds.toml> --output configs/imomoe_server_fixes.cfg
```

The script refuses to overwrite an existing destination file unless `--overwrite` is passed.

---

### Console Commands

#### 1. Check Module Status

Run `sf_status` to inspect each module's enabled state, runtime availability, and diagnostic reason:

```text
sf_status
```

> `sf_status` reports CT and T runtime availability separately in the `hostage_compat` reason string. Overall `hostage_compat available=1` means at least one path is available; an unsupported T gate does not block the independent CT path.

#### How Automatic Expansion Works

The default configuration checks and fills both teams on every map. Set `sf_spawn_capacity 0` to turn off the spawn module; set it back to `1` to rebuild for the current map and player limit. On disable, map change, or unload, the plugin removes its own points and restores team limits it still controls. Limits changed by another plugin or an administrator are preserved, with the reason shown in `sf_status`.

Automatic points reuse existing same-team spawn positions. This lets more players join a team, but players may still spawn at overlapping positions. Use the manual tools below to spread them out. `sf_status` shows `available=1` when both teams have reached the target. If a team has no usable template, it shows `available=0` and asks you to add a point for that team with `sf_spawn_add`; the other team and loaded manual points remain usable. Capacity is recalculated when the player limit changes, each round starts, or manual points are added or removed. The map or mode's team-balance rules still apply.

#### 2. Manual Spawn Point Tools (Server Console Only)

These commands run in the **server console**. They create or manage manual spawn points in memory for the current map (capable of recovering a team even when no valid native source point exists). Use `sf_spawn_save` to persist changes to `cfg/imomoe_server_fixes/spawn_capacity/<map>.cfg` inside the game directory so they are rebuilt on map start before the first team round. `sf_spawn_remove` only removes points created by these tools, never native map points.

```text
sf_spawn_add T 128 -256 32 [pitch yaw roll]
sf_spawn_add CT 128 -256 32 [pitch yaw roll]
sf_spawn_list
sf_spawn_remove <manual-point-id>
sf_spawn_save
```

---

### Build and Test

#### Quick Checks Without the SDK

Requires CMake 3.24+, a C++20 compiler, and Python 3.11+. On Windows, run from a developer shell or pass `-G` to select your installed Visual Studio generator:

```sh
cmake --preset policy
cmake --build --preset policy --parallel 2
ctest --preset policy
python -m unittest discover -s tests -p "test_*.py"
```

The `policy` preset separates Windows and Linux build directories automatically. Put personal generator preferences or local SDK paths in `CMakeUserPresets.json` (ignored by Git).

#### Build the Plugin with the Paired SDK

Building the full plugin requires a C++23 compiler and the pinned inputs documented in [Dependency inputs](docs/DEPENDENCIES.md):

- **Provider & Bindings Contract**: Build with matching Plugify headers and this repository's checked-in `generated-s2sdk/` headers. `bindings/s2sdk.pplugin` is the 161-method / 6-prototype provider manifest (the 159-method base manifest plus `OnMapStart_Register` and `OnMapStart_Unregister`, which S2SDK 3.1.1 exports with the same no-argument callback ABI as `OnMapEndCallback`). Do not regenerate bindings from the SDK root's `plugify-plugin-s2sdk.pplugin.in` template.
- **Paired SourceSDK & `tier0` Linking**: Join-team retry uses typed `IServerGameClients::ClientCommand` and `CCommand` from the SourceSDK submodule pinned by S2SDK (`f3de33c58595c628c12b4ffaba76dfcd49b8763f`). Supply the `public` headers and the configured `build` directory from that same S2SDK worktree (including `<build>/external/sourcesdk/protos` and the static `tier0_perproject` archive).
  - **Windows** links `build/external/sourcesdk/Release/tier0_perproject.lib` and `source/external/sourcesdk/lib/win64/tier0.lib` (resolved at runtime from `game/bin/win64/tier0.dll`).
  - **Steam Runtime Linux** builds with the Sniper GCC 14 toolchain and links `build/external/sourcesdk/libtier0_perproject.a` and `source/external/sourcesdk/lib/linuxsteamrt64/libtier0.so` (resolved at runtime from `game/bin/linuxsteamrt64/libtier0.so`).

**Windows (PowerShell) Build Example:**

```powershell
$pairedS2sdkWorktree = "<paired-pinned-s2sdk-build-worktree>"
$pairedS2sdk = Join-Path $pairedS2sdkWorktree "source"
$plugifyInclude = Join-Path $pairedS2sdk "external/plugify/include"
$generatedS2sdkInclude = Join-Path (Get-Location) "generated-s2sdk/external/plugify/include"
$sourceSdk = Join-Path $pairedS2sdk "external/sourcesdk"
$sourceSdkPublicInclude = Join-Path $sourceSdk "public"
$sourceSdkLibrary = Join-Path $sourceSdk "lib/win64"
$sourceSdkBuild = Join-Path $pairedS2sdkWorktree "build"
$sourceSdkProtoInclude = Join-Path $pairedS2sdkWorktree "build/external/sourcesdk/protos"
$tier0ImportLibrary = Join-Path $sourceSdkLibrary "tier0.lib"
$tier0PerprojectLibrary = Join-Path $sourceSdkBuild "external/sourcesdk/Release/tier0_perproject.lib"

if (-not (Test-Path -LiteralPath $tier0ImportLibrary)) { throw "Missing fixed SourceSDK import library: $tier0ImportLibrary" }
if (-not (Test-Path -LiteralPath $tier0PerprojectLibrary)) { throw "Missing paired SourceSDK static CCommand library: $tier0PerprojectLibrary" }
if (-not (Test-Path -LiteralPath (Join-Path $sourceSdkProtoInclude "netmessages.pb.h"))) { throw "Missing paired SourceSDK protocol headers: $sourceSdkProtoInclude" }

# Optional binding regeneration (only from this repository's bindings/s2sdk.pplugin):
# plugify-gen -lang cpp -manifest (Join-Path (Get-Location) "bindings/s2sdk.pplugin") -output generated-s2sdk

cmake -S . -B build `
  "-DSERVER_FIXES_PLUGIFY_INCLUDE_DIR=$plugifyInclude" `
  "-DSERVER_FIXES_S2SDK_GENERATED_INCLUDE_DIR=$generatedS2sdkInclude" `
  "-DSERVER_FIXES_SOURCE_SDK_PUBLIC_INCLUDE_DIR=$sourceSdkPublicInclude" `
  "-DSERVER_FIXES_SOURCE_SDK_BUILD_DIR=$sourceSdkBuild" `
  "-DSERVER_FIXES_SOURCE_SDK_LIBRARY_DIR=$sourceSdkLibrary" `
  "-DSERVER_FIXES_SOURCE_SDK_PROTO_INCLUDE_DIR=$sourceSdkProtoInclude"
cmake --build build --config Release
```

---

### Compatibility and Technical Notes

- **Pinned CS2 Server Build**: Binary-derived hostage paths and gamedata signatures target **Build 26393695 / ServerVersion 2000940 / PatchVersion 1.41.9.5**:
  - Windows `server.dll`: Timestamp `0x6AE91E94` / Image Size `0x257F000`
  - Linux `libserver.so`: GNU Build ID `b9eac3d151886d4fc820d2e1d1f1d8642daa54c9`
  - Unrecognized server builds fail closed and report the unsupported path in `sf_status`.
- **CT Rescue Mid-Hook Anchor**: Because PolyHook follows a hook placed directly on a `CALL` instruction into the callee, the pawn-rescue dispatch marker is anchored on the immediately preceding argument `MOV` (`mov rcx,rdi` on Windows; `mov rdi,rbx` on Linux). The plugin verifies the instruction bytes, adjacent direct call, return path, and exact callee prologue before attaching, and suppresses compensation if the callee is detoured or if an unclassifiable Hostage API call occurs during the touch.
- **Spawn Registration & Retirement**: Template spawn sources must pass both native geometry qualification and membership in the engine's primary spawn list. Runtime updates invoke the native null-point recount path (`RegisterOrRecountSpawnPoints(nullptr)`), which checks full handles and enabled state without rejecting positions occupied by live players. On map change, disable, or unload, only plugin-owned spawn entities are disabled, recounted, and queued for engine deletion.
- **Map-End Spectator Restoration**: At `OnMapEnd`, while the raw `mp_spectators_max` ConVar is still safe to write, the module restores it if still owned by this plugin so the next map's `CCSGameRules` instance initializes from the prior baseline without querying the tearing-down GameRules object.
- **Earlier Validation (Before the All-Map Expansion Change)**: Windows and Steam Runtime Linux builds have passed policy/callback tests, minimal-plugin checks, and in-game hostage rescue / C4 verification. Two-client pre-ingame chat blocking, full 64-slot capacity (1 human + 63 bots), and spectator lifecycle checks were verified on Windows. Linux runtime deployments require the paired PolyHook repair described in [Dependency inputs](docs/DEPENDENCIES.md#linux-polyhook-range-repair).

### License

The project source licensed under **AGPL-3.0-or-later** (see [`LICENSE`](LICENSE)). Third-party files, such as the S2SDK bindings, retain their own license notices.
