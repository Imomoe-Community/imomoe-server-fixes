# Imomoe Server Fixes (`imomoe_server_fixes`)

[简体中文](#简体中文) | [English](#english)

---

## 简体中文

`imomoe_server_fixes` 是一个面向 Counter-Strike 2 (CS2) 专用服务器的独立 C++ [Plugify](https://github.com/untrustedmodders/plugify) 插件，专门负责处理服务端层面的底层修复与容量兼容问题。它不依赖 `FunRounds` 或 `funrounds_native`，既可以单独部署，也可以与支持桥接的 `FunRounds` 配合运行。

### 功能概览

各模块可独立开关：

| 模块标识 | 控制 ConVar | 默认值 | 功能说明 |
| --- | --- | --- | --- |
| `chat_admission` | `sf_chat_admission` | `1` | **聊天准入拦截**：拦截尚未完全进入游戏（未达到 `InGame` 状态）的真实客户端槽位发送的 `say`、`say_team` 及 SDK 聊天别名指令；RCON、服务端命令、非聊天命令和已正常进服的玩家不受影响。触发拦截时仅在日志记录槽位号与规范命令名。 |
| `hostage_compat` | `sf_hostage_compat` | `1` | **人质救援与背负兼容**：<br>1. **CT 救援触发补偿**：当存活的 CT 背负人质触碰救援区（`CBaseEntity::Touch`）且已到达 pawn 救援派发点，但原生 `CHostage_API::OnRescueZoneTouch` 未被触发时，在核验地图、回合、存活状态、救援区及实体句柄后，自动补发一次原生 `OnRescueZoneTouch` 输入。资金奖励、事件广播与回合结算仍由引擎原生逻辑负责。<br>2. **T 阵营背起人质**：仅当 `mp_only_cts_rescue_hostages` 明确为 `0` (`false`) 时，修改原生人质交互目标选择器的阵营判断以允许 T 背起人质；为 `1` 或未知时保持原生仅限 CT 的行为。关闭模块时会安全还原补丁，且不会打断 T 正在背负的人质。 |
| `spawn_capacity` | `sf_spawn_capacity` | `1` | **出生点容量扩展**：所有地图都会自动补齐 T、CT 双方的出生点，使每队可容纳服务器的 `maxplayers` 人；较低的原生队伍人数上限也会相应提高。玩家因出生点不足而加队失败时，补点后会重试一次。支持手动添加和保存出生点。 |
| `spectator_compat` | `sf_spectator_compat` | `1` | **观察者容量同步**：自动将 `mp_spectators_max` 与 `CCSGameRules::m_iSpectatorSlotCount` 对齐至服务器实际 `GetMaxClients()`（`maxplayers`）。运行中手动修改 `mp_spectators_max` 后，GameRules 改为跟随该值（包括 `0`），换图后继续生效；将 `sf_spectator_compat` 设为 `0` 再设回 `1` 可恢复自动模式。若其他插件直接将 GameRules 改为不同值，本模块会在本次启用周期内停止同步该字段。关闭、换图或卸载时只还原仍由本插件持有的值。 |
| `game_ban_fix` | `sf_game_ban_fix` | `0` | **游戏封禁残留修复**：核对当前连接的 Steam 认证身份，防止旧控制器状态触发误踢；清理已过期的 GC 封禁，保留有效封禁和通信状态。 |
| `beam_fix` | `sf_beam_fix` | `1` | **光束坐标卡死修复**：修复无父级实体的光束、激光（`CBeam`）设置起始或终点坐标时的卡死与崩溃。 |
| `filter_activator_fix` | `sf_filter_activator_fix` | `1` | **过滤器无触发者修复**：忽略 `filter_*` 实体上缺少触发者（activator 为空）的 `TestActivator` 输入，防止误触发通过或失败输出。 |
| `equip_precache_fix` | `sf_equip_precache_fix` | `1` | **装备预缓存跳过**：在 `game_player_equip` 预缓存时跳过 `item_*` 类装备，避免引擎异常并保留地图原始配置与发放逻辑。 |
| `free_client_fix` | `sf_free_client_fix` | `0` | **空闲客户端槽位修复**：暂不可用；在确认错误拒绝对应的槽位可安全复用之前，保留引擎原生行为。 |

三个实体修复在受支持的构建上默认启用。用 `sf_status` 查看各 Hook 是否可用；不要与 CS2Fixes 的同类 Hook 同时启用。详见[原生实体修复与底层保护](#原生实体修复与底层保护)及[原生修复说明文档](docs/NATIVE_FAULTS.md)。

#### 与 FunRounds 的职责桥接

插件导出了 `GetOwnedFixes()` 接口并固定返回掩码 `3`（bit 0 代表 `chat_admission`，bit 1 代表 `hostage_compat`），用于通知支持所有权桥接的 `FunRounds` 由本插件接管这两项功能（返回值不随模块开关或运行期可用性变化）。其他模块独立运行，不占用 `FunRounds` 掩码位。

---

### 安装与配置

1. 将插件包部署至 Plugify 插件目录，并确保运行环境所使用的 S2SDK 插件清单（`.pplugin`）声明了 `OnMapStart_Register` 与 `OnMapStart_Unregister`（可直接采用本仓库提供的 [`bindings/s2sdk.pplugin`](bindings/s2sdk.pplugin)）。
2. 将 [`configs/imomoe_server_fixes.cfg`](configs/imomoe_server_fixes.cfg) 复制到游戏目录的 `game/csgo/cfg/imomoe_server_fixes.cfg`：
   ```cfg
   sf_chat_admission 1
   sf_hostage_compat 1
   sf_spawn_capacity 1
   sf_spectator_compat 1
   sf_game_ban_fix 0
   sf_beam_fix 1
   sf_filter_activator_fix 1
   sf_equip_precache_fix 1
   sf_free_client_fix 0
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

在控制台执行 `sf_status` 可实时查看各模块的启用状态（`enabled`）、运行期可用性（`available`）及具体原因（`reason`）：

```text
sf_status
```

> - 对于 `hostage_compat`，`sf_status` 会在 `reason` 中分别报告 CT 救援补偿路径与 T 背负补丁路径的可用性；只要其中任意一条路径可用，`available` 即为 `1`。
> - 对于 `beam_fix`、`filter_activator_fix`、`equip_precache_fix` 与 `free_client_fix`，`sf_status` 会实时报告各底层 Hook 的装载状态；若特征码不匹配、live 代码已被修改或存在冲突 Detour，将显示具体不可用原因。

#### 原生实体修复与底层保护

在支持的 CS2 服务端版本上，插件通过 Gamedata 签名与 PolyHook 提供了一组底层原生修复，默认自动生效（`sf_beam_fix`、`sf_filter_activator_fix`、`sf_equip_precache_fix`）：

1. **光束坐标卡死修复 (`sf_beam_fix`)**：
   - 原生 CS2 中，当地图实体或脚本对无父级实体（Unparented）的光束或激光（`CBeam`）设置起始原点（`SetOrigin`）或终点坐标（`SetEndPos`）时，引擎沿用父级层级分发逻辑会导致死锁或崩溃。
   - 修复逻辑：Detour 拦截 `CBeam::SetOrigin` 与 `CBeam::SetEndPos`；若该实体存在父级（`BeamGetParent`），则完全交由引擎原生逻辑处理；若不存在父级，将本地原点直接送入原生物理/场景写入函数（`BeamWriteLocalOrigin`），并对终点坐标触发 `NetworkStateChanged(m_vecEndPos)` 网络同步，消除卡死风险。
2. **过滤器空触发者修复 (`sf_filter_activator_fix`)**：
   - 当地图实体或逻辑触发器向 `filter_*` 实体发送 `TestActivator` 输入且未传递有效 Activator（Activator 为空指针，例如由计时器或中继触发）时，原生逻辑在空指针判断上存在缺陷，可能误触发 `OnPass` 或 `OnFail` 输出导致地图流程紊乱。
   - 修复逻辑：Detour 拦截 `FilterAcceptInput`，当输入名匹配 `TestActivator` 且 Activator 为空时，直接消费该输入（返回 `true` 并中断后续分发），阻止错误的判定输出。
3. **装备预缓存跳过 (`sf_equip_precache_fix`)**：
   - 在地图加载与实体预缓存（Precache）阶段，`game_player_equip` 会枚举待发放装备名。当配置中包含 `item_*` 类装备时，原生预缓存查找可能导致非预期的引擎告警或异常。
   - 修复逻辑：通过 Mid-Hook 在 `PlayerEquipPrecacheLookup` 阶段拦截寄存器中的装备名称查找结果，仅在 Precache 阶段将 `item_*` 类名称替换为空字符串以安全跳过原生预缓存调用；地图实际生成与玩家生成时的正式装备发放逻辑不受任何影响。
4. **底层代码校验与防冲突保障 (`ServerCodeUnmodified`)**：
   - 在挂载任何 Detour 或 Mid-Hook 之前，插件会通过 `ServerCodeUnmodified` 读取磁盘上的原生可执行文件（Windows 为 `server.dll`，Linux 为 `libserver.so`），将待 Hook 区域的内存指令与磁盘原始二进制进行精确比对（最大覆盖 PolyHook 23 字节的覆写窗口）。
   - 若检测到目标函数已被其他插件（如 CS2Fixes）挂载 Detour、内存指令不匹配或文件被篡改，模块将立即安全放弃挂载（Fail-closed），并在 `sf_status` 中明确提示原因，绝不强行覆盖或引发双重 Hook 崩溃。更多细节请参考[原生修复说明文档](docs/NATIVE_FAULTS.md)。

#### 游戏封禁残留修复

在隔离服用 `sf_game_ban_fix 1` 启用，然后用 `sf_status` 确认 `game_ban_fix available=1`。目前支持 Windows/Linux 的 CS2 Build 26445667。不要与其他 GameBanFix/steam_ban detour 同时启用。签名、构建或 Hook 不匹配时仅停用本模块；修正环境后切换 `0` → `1` 重试。

插件不改写 `sv_kick_players_with_cooldown`。值为 `0` 时保留引擎自身的安全服回退规则；低于 `2` 时仅在本次检查中屏蔽竞技冷却原因 `20`、`22`、`23`；值为 `2` 时由引擎处理所有原生可执行的冷却。有效的 GC 封禁保留到过期，以便封禁账号重连时仍会被原生逻辑拒绝。服务端 `banid` 列表与持久化文件不由本模块修改。

Windows 和 Linux 各在隔离服验证一次：

1. `sv_kick_players_with_cooldown 1`：有效游戏封禁仍触发原生拒绝；只有竞技冷却的账号和正常账号可加入。检查失败连接、连续进服和换图后重连，确认正常账号不会触发 `STEAM_ID_PENDING` 封禁循环。
2. 切到 `2`：竞技冷却账号仍按原生配置被拒绝；切到 `0`：与同一安全模式下关闭模块的原生行为比较。确认管理员封禁仍生效。
3. 切换功能开关、重载插件并换图，确认没有重复 Hook。在单独的隔离服副本中移除 GameBanFix 的签名，确认 `sf_status` 显示其不可用，其他模块和服务端继续工作。

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
python -m unittest discover -s tools/tests -p "test_*.py"
```

`policy` 预设会将 Windows 与 Linux 的构建产物隔离在不同目录中。若需配置本地特定的生成器或 SDK 路径，请写入本地的 `CMakeUserPresets.json`（已被 Git 忽略）。在具备运行时依赖的环境中，回调测试集会自动覆盖 `chat_listener`、`hostage_policy`、`spawn_capacity`、`spectator_compat`、`game_ban_fix` 以及 `native_faults`。

#### 2. 使用配对 SDK 构建完整插件

编译完整插件需要支持 C++23 的编译器，并使用 [依赖说明 (docs/DEPENDENCIES.md)](docs/DEPENDENCIES.md) 中固定的依赖版本与配对构建目录：

- **绑定与清单契约**：请使用配对 S2SDK 的 Plugify 头文件以及本仓库检入的 `generated-s2sdk/` 导出头。`bindings/s2sdk.pplugin` 是在 159 方法 / 6 原型清单的基础上，额外声明了 S2SDK 3.3.1 已实现的 `OnMapStart_Register` 与 `OnMapStart_Unregister`（共 161 方法 / 7 原型）。请勿使用 S2SDK 仓库根目录的 `plugify-plugin-s2sdk.pplugin.in` 模板重新生成。
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

#### 3. 一键配对构建与打包

在 Windows 与 Linux 环境下可通过统一脚本完成完整发布包的构建与打包。脚本会自动校验固定的 S2SDK，运行 Python、策略与回调测试，编译生产构建插件，并将清单、Gamedata、配置以及配对依赖许可打包为最终分发 ZIP。Linux 构建与回调测试将在固定的 Steam Runtime 容器中执行。

主机环境需 Python 3.11+、Git、CMake、Ninja（Linux）以及 C++23 工具链（Windows 为 Visual Studio x64，Linux 为 Docker）。需在 `.work/tools/plugify-gen-v1.2.15/` 下安装 `plugify-gen` 1.2.15 或通过 `--generator PATH` 指定路径。回调测试需要加载真实的 CS2 `tier0` 运行时，请通过 `--cs2-bin` 传入 `game/bin/win64` 或 `game/bin/linuxsteamrt64` 路径，或设置 `CS2_GAME_BIN` 环境变量。

**Windows (PowerShell)：**

```powershell
python tools/build_paired.py --funrounds-source D:/git/Imomoe-FunRounds-CS2 --cs2-bin D:/path/to/game/bin/win64
```

**Linux/WSL (x64 带 Docker)：**

```bash
python3 tools/build_paired.py --funrounds-source /path/to/Imomoe-FunRounds-CS2 --cs2-bin /path/to/game/bin/linuxsteamrt64
```

产物将输出至 `dist/imomoe-server-fixes-<platform>-<name>.zip`。

---

### 版本兼容与技术细节

- **目标 CS2 服务端版本**：当前二进制特征码、偏移和原生修复钩子绑定至 **Build 26445667 / ServerVersion 2000942 / PatchVersion 1.41.9.6**：
  - Windows `server.dll`：Timestamp `0x6AEBC501` / Image Size `0x257C000`
  - Linux `libserver.so`：GNU Build ID `5ea0e482b46eb71c4f9615c48b34ddc36f55ca40`
  - 当服务端二进制与上述标识不匹配时，依赖特征码或内存补丁的路径会自动安全禁用（Fail-closed），并通过 `sf_status` 报告原因。
- **动态 Gamedata 与二进制完整性校验**：所有特征码与字段偏移均由 [`gamedata/imomoe_server_fixes.jsonc`](gamedata/imomoe_server_fixes.jsonc) 集中管理。在安装任何 PolyHook Detour 或内存补丁前，插件通过 `ServerCodeUnmodified` 读取磁盘原始文件，对比目标内存区间指令（最大覆盖 23 字节跳板覆写窗口），防止在已修改或被外部插件 Detour 的代码上重复下钩。
- **CT 救援检测机制**：通过观察 `CBaseEntity::Touch`、派发器直接调用 pawn 救援函数前一条传参 `MOV` 指令上的寄存器快照 Mid-Hook（Windows 为 `mov rcx,rdi`，Linux 为 `mov rdi,rbx`，之所以锚定在 `MOV` 是因为 PolyHook 对 `CALL` 指令下钩子时会跟随进入被调函数），以及 `CHostage_API::OnRescueZoneTouch` 入口来判断救援是否被引擎正常处理。挂载前会校验指令字节、相邻直接调用、返回路径及被调函数序言；若存在外部 Detour 或无法明确分类的调用，则放弃当次补偿以避免误触发。
- **人质背负与阵营补丁**：T 阵营人质拾取补丁通过 Gamedata 签名定位 `ServerFixes::HostageTeamGate` 并完成字节级原子切换与保护还原，仅在 `mp_only_cts_rescue_hostages` 为 `0` 时激活。
- **出生点生命周期与重算**：自动补充的出生点要求模板源同时通过原生几何空间校验并位于引擎主出生点列表中；运行期增删出生点会调用原生 `RegisterOrRecountSpawnPoints(nullptr)` 路径重算有效容量（仅校验完整句柄与启用状态，不会因为当前位置站有存活玩家而误判失效）。在换图、禁用或卸载时，插件会先禁用自建出生点并触发重算，再交由 S2SDK 请求引擎安全移除。
- **观察者上限换图恢复**：在地图结束（`OnMapEnd`）且原生 ConVar 仍处于安全可写阶段时，若 `mp_spectators_max` 仍由本插件持有，插件会先将其恢复为接管前的原始值，从而确保下一张地图的 `CCSGameRules` 实例从原始基准值初始化，而无需访问正在销毁的旧 GameRules 对象。
- **已有版本验证记录**：Windows 与 Steam Runtime Linux 均已通过完整构建、策略与回调测试、最小插件验证、真机人质救援/C4 控制验收以及原生实体修复（`beam_fix`、`filter_activator_fix`、`equip_precache_fix`）加载和冲突验证；Windows 已完成双客户端聊天拦截、满员 64 人（1 真人 + 63 BOT）容量及观察者死亡/掉落/视角切换/返回全流程验收。当前运行配对见 [依赖说明](docs/DEPENDENCIES.md)；Linux `2.5.7` 旧包须包含 [PolyHook 页边界修复](docs/DEPENDENCIES.md#linux-polyhook-257-fallback)。

### 开源协议

本项目源码采用 **AGPL-3.0-or-later** 协议，详见 [`LICENSE`](LICENSE)。S2SDK 绑定等第三方文件保留各自的许可声明。

---

## English

`imomoe_server_fixes` is a standalone C++ [Plugify](https://github.com/untrustedmodders/plugify) plugin for Counter-Strike 2 (CS2) dedicated servers. It owns server-level fixes and capacity compatibility without importing `FunRounds` or `funrounds_native`, and can run either on its own or alongside a bridge-aware `FunRounds` installation.

### What It Owns

Each module can be toggled independently:

| Module | ConVar | Default | Description |
| --- | --- | --- | --- |
| `chat_admission` | `sf_chat_admission` | `1` | **Chat Admission**: Blocks `say`, `say_team`, and the SDK Chat alias path for real client slots that are not yet `InGame`. Server/RCON commands, non-chat commands, and fully connected players pass through unchanged. Logs record only the slot number and canonical command label. |
| `hostage_compat` | `sf_hostage_compat` | `1` | **Hostage Compatibility**:<br>1. **CT Rescue Compensation**: Observes `CBaseEntity::Touch`, a build-guarded mid-hook immediately before the dispatcher's direct pawn-rescue call, and the native `CHostage_API::OnRescueZoneTouch` target-input entry. If the pawn rescue call site is reached with certain observation and `OnRescueZoneTouch` was not entered for the carried hostage on that Touch stack, Touch Post revalidates the map, round, player life, rescue zone, carried hostage, and prop before submitting a single native `OnRescueZoneTouch` input. Rewards, events, counters, cleanup, and round outcomes remain handled by the engine.<br>2. **T Hostage Pickup**: Patches the native hostage target selector's team check only when `mp_only_cts_rescue_hostages` is known to be `false`; `true` or unknown values keep the native CT-only check. Disabling the module restores the original selector bytes without interrupting a hostage already carried by a Terrorist. |
| `spawn_capacity` | `sf_spawn_capacity` | `1` | **Spawn Capacity**: Automatically fills T and CT spawn capacity on every map so either team can hold `maxplayers` players. Raises lower native team limits as needed. Retries a join request once after adding capacity for a spawn-related rejection. Includes tools to add and save spawn points manually. |
| `spectator_compat` | `sf_spectator_compat` | `1` | **Spectator Capacity**: Keeps `mp_spectators_max` and `CCSGameRules::m_iSpectatorSlotCount` aligned with `GetMaxClients()` (`maxplayers`). If `mp_spectators_max` is changed while running, GameRules follows that value (including `0`) across map changes. Toggle `sf_spectator_compat` off and back on to resume automatic mode. A conflicting direct GameRules override suspends synchronization for the rest of the enabled cycle. On disable, map end, or unload, the module restores only values it still owns. |
| `game_ban_fix` | `sf_game_ban_fix` | `0` | **Game Ban Fix**: Verifies the live Steam connection before native GC enforcement and removes expired GC sanctions without dropping active bans or communication state. |
| `beam_fix` | `sf_beam_fix` | `1` | **Beam Coordinate Hang Fix**: Repairs origin and endpoint updates for unparented beams and lasers (`CBeam`), avoiding engine deadlocks and crashes. |
| `filter_activator_fix` | `sf_filter_activator_fix` | `1` | **Filter Activator Guard**: Consumes `filter_*` `TestActivator` inputs without an activator, preventing false pass or fail outputs. |
| `equip_precache_fix` | `sf_equip_precache_fix` | `1` | **Equip Precache Filter**: Skips `item_*` classes only during `game_player_equip` precaching, avoiding engine precache errors while preserving map configurations and actual item grants. |
| `free_client_fix` | `sf_free_client_fix` | `0` | **Free Client Fix**: Unavailable until native rejections are confirmed safe for slot reuse; keeps engine behavior untouched. |

The three entity fixes default on for supported builds. Use `sf_status` to check hook availability, and do not enable overlapping CS2Fixes hooks alongside them. See [Native Entity Fixes and Low-Level Safety](#native-entity-fixes-and-low-level-safety) and [native fix details](docs/NATIVE_FAULTS.md).

#### FunRounds Ownership Bridge

`GetOwnedFixes()` always returns `3`: bit 0 is `chat_admission` and bit 1 is `hostage_compat`. This mask stays constant regardless of whether individual modules are enabled or available at runtime. `spawn_capacity` and `spectator_compat` are standalone capabilities and do not add FunRounds ownership bits.

---

### Configure and Check Status

1. Install the plugin and ensure the runtime S2SDK provider descriptor declares `OnMapStart_Register` and `OnMapStart_Unregister` (use this repository's [`bindings/s2sdk.pplugin`](bindings/s2sdk.pplugin) for the standalone installation).
2. Copy [`configs/imomoe_server_fixes.cfg`](configs/imomoe_server_fixes.cfg) to `game/csgo/cfg/imomoe_server_fixes.cfg`:
   ```cfg
   sf_chat_admission 1
   sf_hostage_compat 1
   sf_spawn_capacity 1
   sf_spectator_compat 1
   sf_game_ban_fix 0
   sf_beam_fix 1
   sf_filter_activator_fix 1
   sf_equip_precache_fix 1
   sf_free_client_fix 0
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

> - `sf_status` reports CT and T runtime availability separately in the `hostage_compat` reason string. Overall `hostage_compat available=1` means at least one path is available; an unsupported T gate does not block the independent CT path.
> - For `beam_fix`, `filter_activator_fix`, `equip_precache_fix`, and `free_client_fix`, `sf_status` reports the live hook status and specific failure reasons (such as signature mismatch, modified memory code, or existing foreign detours).

#### Native Entity Fixes and Low-Level Safety

On supported CS2 builds, the plugin provides low-level native fixes through Gamedata signatures and PolyHook detours, enabled by default (`sf_beam_fix`, `sf_filter_activator_fix`, `sf_equip_precache_fix`):

1. **Unparented Beam Coordinate Hang Fix (`sf_beam_fix`)**:
   - In native CS2, setting the origin (`SetOrigin`) or endpoint (`SetEndPos`) on an unparented `CBeam` entity causes deadlocks or crashes during scene/physics traversal because the engine assumes a valid parent entity exists.
   - Detours intercept `CBeam::SetOrigin` and `CBeam::SetEndPos`. If the entity has a parent (`BeamGetParent`), it is passed through to native code. If unparented, the origin is passed directly to the native local origin writer (`BeamWriteLocalOrigin`), and endpoint updates trigger `NetworkStateChanged(m_vecEndPos)` directly.
2. **Filter TestActivator Guard (`sf_filter_activator_fix`)**:
   - When a map trigger fires `TestActivator` on a `filter_*` entity without passing an activator (`nullptr`), native logic misinterprets the null pointer, often triggering false `OnPass` or `OnFail` outputs.
   - Detours intercept `FilterAcceptInput`, consume the input directly by returning `true` (`Supercede`) when the activator is null and input name is `TestActivator`, suppressing erroneous output triggers.
3. **Equipment Item Precaching Filter (`sf_equip_precache_fix`)**:
   - During map precaching, `game_player_equip` enumerates item names. When items begin with `item_*`, the engine precache routine cannot resolve them, generating unnecessary warnings or errors.
   - A mid-hook at `PlayerEquipPrecacheLookup` replaces the item name in `RAX` with an empty string during precache lookup, cleanly skipping precaching while preserving entity keyvalues and actual spawn grants.
4. **Code Verification and Collision Avoidance (`ServerCodeUnmodified`)**:
   - Before attaching any detour or mid-hook, the plugin reads the on-disk server binary (`server.dll` on Windows, `libserver.so` on Linux) to compare instruction bytes at the target address against disk bytes (up to 23 bytes for PolyHook overwrites).
   - If an existing detour from another plugin (e.g., CS2Fixes) is detected via `polyhook::FindDetour` or bytes are modified, the module fails closed and records the reason in `sf_status`. See [native fix details](docs/NATIVE_FAULTS.md).

#### Game Ban Fix

On a test server, enable `sf_game_ban_fix 1` and check `game_ban_fix available=1` in `sf_status`. The fix targets CS2 Build 26445667 on Windows and Linux. Do not enable another GameBanFix/steam_ban detour at the same time. If the build, signatures, or hooks do not match, only this module stays inactive. After correcting the setup, toggle it off and back on to retry.

The fix leaves `sv_kick_players_with_cooldown` unchanged. At `0` the engine retains its secure-server fallback; below `2`, reasons `20`, `22`, and `23` are hidden only during the check. At `2` the engine applies its normal cooldown policy. Active sanctions remain enforceable on reconnect. The fix does not edit the server's ban list or ban files. For the live acceptance steps, see [游戏封禁残留修复](#游戏封禁残留修复).

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

#### 1. Quick Checks Without the SDK

Requires CMake 3.24+, a C++20 compiler, and Python 3.11+. On Windows, run from a developer shell or pass `-G` to select your installed Visual Studio generator:

```sh
cmake --preset policy
cmake --build --preset policy --parallel 2
ctest --preset policy
python -m unittest discover -s tests -p "test_*.py"
python -m unittest discover -s tools/tests -p "test_*.py"
```

The `policy` preset separates Windows and Linux build directories automatically. Put personal generator preferences or local SDK paths in `CMakeUserPresets.json` (ignored by Git). With runtime dependencies present, callback test targets cover `chat_listener`, `hostage_policy`, `spawn_capacity`, `spectator_compat`, `game_ban_fix`, and `native_faults`.

#### 2. Build the Plugin with the Paired SDK

Building the full plugin requires a C++23 compiler and the pinned inputs documented in [Dependency inputs](docs/DEPENDENCIES.md):

- **Provider & Bindings Contract**: Build with matching Plugify headers and this repository's checked-in `generated-s2sdk/` headers. `bindings/s2sdk.pplugin` is the 161-method / 7-prototype provider manifest (the 159-method base manifest plus `OnMapStart_Register` and `OnMapStart_Unregister`, which S2SDK 3.3.1 exports with the no-argument `OnMapStartCallback` prototype). Do not regenerate bindings from the SDK root's `plugify-plugin-s2sdk.pplugin.in` template.
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

#### 3. 一键配对构建与打包 / One-Command Paired Builds

在 Windows 与 Linux 环境下可通过统一脚本完成完整发布包的构建与打包。脚本会自动校验固定的 S2SDK，运行 Python、策略与回调测试，编译生产构建插件，并将清单、Gamedata、配置以及配对依赖许可打包为最终分发 ZIP。Linux 构建与回调测试将在固定的 Steam Runtime 容器中执行。

Run the same entry on Windows and Linux to build the complete plugin ZIP for each platform. It checks the pinned S2SDK, runs ServerFixes' Python, policy, and callback tests, builds the production plugin, and packages the manifest, gamedata, config, and paired dependency licenses. Linux compilation and callback tests run in the pinned Steam Runtime container.

The host needs Python 3.11+, Git, CMake, Ninja on Linux, and the project C++23 toolchain (Visual Studio x64 on Windows; Docker on Linux). Install `plugify-gen` 1.2.15 in `.work/tools/plugify-gen-v1.2.15/` or pass `--generator PATH`. Callback tests load the real CS2 `tier0` runtime: pass the matching `game/bin/win64` or `game/bin/linuxsteamrt64` directory through `--cs2-bin`, or set `CS2_GAME_BIN`.

**Windows (PowerShell):**

```powershell
python tools/build_paired.py --funrounds-source D:/git/Imomoe-FunRounds-CS2 --cs2-bin D:/path/to/game/bin/win64
```

**Linux/WSL (x64 with Docker):**

```bash
python3 tools/build_paired.py --funrounds-source /path/to/Imomoe-FunRounds-CS2 --cs2-bin /path/to/game/bin/linuxsteamrt64
```

The verified ZIP is written to `dist/imomoe-server-fixes-<platform>-<name>.zip`.

---

### 版本兼容与技术细节 / Compatibility and Technical Notes

- **目标 CS2 服务端版本 / Pinned CS2 Server Build**：当前二进制特征码、偏移和原生修复钩子绑定至 **Build 26445667 / ServerVersion 2000942 / PatchVersion 1.41.9.6**：
  - Windows `server.dll`：Timestamp `0x6AEBC501` / Image Size `0x257C000`
  - Linux `libserver.so`：GNU Build ID `5ea0e482b46eb71c4f9615c48b34ddc36f55ca40`
  - 当服务端二进制与上述标识不匹配时，依赖特征码或内存补丁的路径会自动安全禁用（Fail-closed），并通过 `sf_status` 报告原因。
- **动态 Gamedata 与二进制完整性校验**：所有特征码与字段偏移均由 [`gamedata/imomoe_server_fixes.jsonc`](gamedata/imomoe_server_fixes.jsonc) 集中管理。在安装任何 PolyHook Detour 或内存补丁前，插件通过 `ServerCodeUnmodified` 读取磁盘原始文件，对比目标内存区间指令（最大覆盖 23 字节跳板覆写窗口），防止在已修改或被外部插件 Detour 的代码上重复下钩。
- **CT 救援检测机制**：通过观察 `CBaseEntity::Touch`、派发器直接调用 pawn 救援函数前一条传参 `MOV` 指令上的寄存器快照 Mid-Hook（Windows 为 `mov rcx,rdi`，Linux 为 `mov rdi,rbx`，之所以锚定在 `MOV` 是因为 PolyHook 对 `CALL` 指令下钩子时会跟随进入被调函数），以及 `CHostage_API::OnRescueZoneTouch` 入口来判断救援是否被引擎正常处理。挂载前会校验指令字节、相邻直接调用、返回路径及被调函数序言；若存在外部 Detour 或无法明确分类的调用，则放弃当次补偿以避免误触发。
- **人质背负与阵营补丁**：T 阵营人质拾取补丁通过 Gamedata 签名定位 `ServerFixes::HostageTeamGate` 并完成字节级原子切换与保护还原，仅在 `mp_only_cts_rescue_hostages` 为 `0` 时激活。
- **出生点生命周期与重算**：自动补充的出生点要求模板源同时通过原生几何空间校验并位于引擎主出生点列表中；运行期增删出生点会调用原生 `RegisterOrRecountSpawnPoints(nullptr)` 路径重算有效容量（仅校验完整句柄与启用状态，不会因为当前位置站有存活玩家而误判失效）。在换图、禁用或卸载时，插件会先禁用自建出生点并触发重算，再交由 S2SDK 请求引擎安全移除。
- **观察者上限换图恢复**：在地图结束（`OnMapEnd`）且原生 ConVar 仍处于安全可写阶段时，若 `mp_spectators_max` 仍由本插件持有，插件会先将其恢复为接管前的原始值，从而确保下一张地图的 `CCSGameRules` 实例从原始基准值初始化，而无需访问正在销毁的旧 GameRules 对象。
- **已有版本验证记录**：Windows 与 Steam Runtime Linux 均已通过完整构建、策略与回调测试、最小插件验证、真机人质救援/C4 控制验收以及原生实体修复（`beam_fix`、`filter_activator_fix`、`equip_precache_fix`）加载和冲突验证；Windows 已完成双客户端聊天拦截、满员 64 人（1 真人 + 63 BOT）容量及观察者死亡/掉落/视角切换/返回全流程验收。当前运行配对见 [依赖说明](docs/DEPENDENCIES.md)；Linux `2.5.7` 旧包须包含 [PolyHook 页边界修复](docs/DEPENDENCIES.md#linux-polyhook-257-fallback)。

### 开源协议 / License

本项目源码采用 **AGPL-3.0-or-later** 协议，详见 [`LICENSE`](LICENSE)。S2SDK 绑定等第三方文件保留各自的许可声明。
