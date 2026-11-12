# Native Fault Fixes / 原生底层与移动修复说明

[简体中文](#简体中文) | [English](#english)

---

## 简体中文

`imomoe_server_fixes` 通过 Gamedata 定位原生入口，按模块修复实体处理、玩家移动、网络消息和长图时钟问题。按需启用模块后，用 `sf_status` 查看实际可用性。

各模块均具备独立的容错机制：当签名未匹配或目标代码已被外部插件修改时，会自动安全关闭（Fail-closed）并由 `sf_status` 报告具体原因，不影响服务端其他功能。

### 模块机制详解

#### 1. 光束坐标卡死修复 (`sf_beam_fix`，默认 1)

- **引擎缺陷**：当地图或脚本对无父级（Unparented）的 `CBeam`（光束、激光）实体调用坐标设置函数（`SetOrigin` 或 `SetEndPos`）时，原生逻辑在更新物理层级与场景图时假设实体具备父级层级，可能导致服务端死锁、无限循环或崩溃。
- **修复方案**：
  - 拦截 `ServerFixes::BeamSetOrigin` 与 `ServerFixes::BeamSetEndPos`。
  - 首先通过 `ServerFixes::BeamGetParent` 查询实体的父级指针。若实体存在父级，直接忽略（`ResultType::Ignored`），保留引擎原生处理链路。
  - 若实体无父级：
    - `SetOrigin`：直接调用原生本地坐标写入函数（`ServerFixes::BeamWriteLocalOrigin`），绕过错误的父级层级分发；
    - `SetEndPos`：对比当前坐标与目标坐标，若发生改变，调用 `beam->NetworkStateChanged(m_vecEndPos)` 触发网络状态同步，并直接写入目标坐标，返回 `ResultType::Supercede` 阻止原生问题逻辑。

#### 2. 过滤器无触发者修复 (`sf_filter_activator_fix`，默认 1)

- **引擎缺陷**：当实体 I/O 触发器向 `filter_*` 实体发送 `TestActivator` 输入但未传递有效 Activator（指针为空，例如由计时器或中继触发）时，原生判定逻辑存在空指针假设缺陷，可能触发错误的 `OnPass` 或 `OnFail` 输出，导致地图逻辑流程混乱。
- **修复方案**：
  - 拦截 `ServerFixes::FilterAcceptInput`。
  - 检查输入名称是否为 `TestActivator`（大小写不敏感），实体类名是否以 `filter_` 开头，且 Activator 指针是否为 `nullptr`。
  - 命中上述条件时，直接将返回值设为 `true`（表示输入已消费）并返回 `ResultType::Supercede`，阻断原生处理链路，既不执行测试逻辑，也不误触发任何通过/失败输出。

#### 3. 装备预缓存跳过 (`sf_equip_precache_fix`，默认 1)

- **引擎缺陷**：在地图加载和实体预缓存阶段，`game_player_equip` 会枚举待发放的物品类名并尝试预缓存。当类名包含 `item_*`（如护甲等）时，原生物品预缓存机制无法正确解析该类标识，导致非预期的引擎告警或加载异常。
- **修复方案**：
  - 在 `ServerFixes::PlayerEquipPrecacheLookup` 安装寄存器 Mid-Hook（`HookDetour2`）。
  - 监控读取物品名称结果的 `RAX` 寄存器。若物品名称以 `item_` 开头（大小写不敏感），将 `RAX` 临时替换为指向空字符串 `""` 的指针。
  - 原生代码在遇到空字符串时会安全跳过本次预缓存调用；实体的原始 KeyValues 与后续正式生成、装备发放逻辑完全不受影响。

#### 4. 地图实体 Hammer ID 保留 (`sf_hammer_id`，默认 0)

- **引擎缺陷**：CS2 专用服务器在生成部分地图实体时，原生地图键值解析分支会跳过读取并存储 `hammerUniqueId`，使得通过插件或脚本查询实体 Hammer ID 时返回无效或丢失状态。
- **修复方案**：
  - 通过 `OwnedCodePatch` 在 `ServerFixes::HammerIdSkip` 安装代码补丁，将跳过读取的条件跳转指令（JZ）改写为直接直通（Fallthrough）。
  - 原生实体属性与类型判断完整保留，实体初始化后即可正确读取和保留其原始 Hammer ID。需在加载目标地图前开启。

#### 5. CSTV 与 BOT 配额排除 (`sf_cstv_bot_quota`，默认 0)

- **引擎缺陷**：当服务器配置了 BOT 配额（`bot_quota`）且开启了 SourceTV/CSTV（`tv_enable 1`）时，原生的 BOT 配额踢人与挑选逻辑可能将 CSTV 控制器当作候选目标进行处理，引发偶发性 CSTV 掉线或配额平衡异常。
- **修复方案**：
  - 拦截 `ServerFixes::SelectAndKickBot` 与 `ServerFixes::BotTeamCandidate`。
  - 仅在配额挑选目标时核验控制器的 HLTV 标志位（`controller[g_hltvOffset] != 0`），排除 CSTV；不改变 BOT 配额数值，也不阻碍 CSTV 正常的连接与断开。

#### 6. 水中跳跃动量修复 (`sf_water_jump`，默认 0)

- **引擎缺陷**：在玩家执行水中移动（`WaterMove`）离开水面时，引擎会立即紧接着调用一次 `CategorizePosition(..., stayOnGround=false)`。原生逻辑在此处会将刚脱离水面的玩家错误判定为触地，强制重置竖向速度并清空跳跃动量。
- **修复方案**：
  - 挂钩 `ServerFixes::WaterMove` 与 `ServerFixes::CategorizePosition`。
  - 当检测到本次地面分类调用紧随 `WaterMove` 且为相同移动上下文时，单次跳过该次触底分类（`Supercede`）；其他正常地面分类与触底判定照常运行。

#### 7. 单 Tick 跳跃上限 (`sf_jump_spam`，默认 0)

- **引擎缺陷**：同一移动 tick 内重复结算起跳会产生不一致的移动结果。
- **修复方案**：
  - 拦截 `ServerFixes::LegacyJumpAttempt`、`ServerFixes::ModernJumpAttempt` 与 `ServerFixes::CompleteJump`。
  - 追踪每个活体 Pawn 在当前移动 Tick 内实际生效的跳跃事件。同一 Pawn 在同一 Tick 内若已结算过一次原生跳跃，后续尝试将被直接拦截；未成功起跳的尝试不计入配额，完整保留常规连跳服手感。

#### 8. 斜坡接缝与滑行碰撞修复 (`sf_rampbug`，默认 0)

- **引擎缺陷**：在滑坡（Surf）、KZ 或包含大量斜坡/多边形接缝的地图上，原生扫掠（Sweep）追踪可能因接缝处微小法线不一致而报告异常阻挡，导致玩家卡在平整斜坡接缝处（Rampbug）或损失水平速度。
- **修复方案**：
  - 挂钩 `ServerFixes::RampTryPlayerMove`、`ServerFixes::RampCachedHullTrace`、`ServerFixes::RampUncachedHullTrace` 与 `CategorizePosition`。
  - 当玩家沿斜面滑行并在接缝处遇到不一致法线碰撞时，利用追踪的斜坡法线向外微移探测有效路径（`FindCorrection`）；落地支撑面符合坡面标准时依据引擎选中平面恢复水平动量。平地、水中及非步行移动完全不接管。

#### 9. KeyValues 指令限流 (`sf_keyvalues_flood`，默认 0)

- **用途**：限制每个连接的 KeyValues 指令处理开销。
- **修复方案**：
  - Detour 拦截 `engine2` 库中的 `CServerSideClient::ProcessCmdKeyValues`。
  - 基于令牌桶算法（Token Bucket）为每个连接独立计量请求速率（默认 10/s、突发 20、单条限制 1500 字节）。超额或超长指令直接被消费丢弃（返回 `true`），不影响正常游戏连接，且不粗暴踢出玩家。

#### 10. 比赛结束换图恢复 (`sf_blank_map`，默认 0)

- **引擎缺陷**：在特定模式下比赛结束（Match End）时，若配置未提供下一个目标地图且引擎自身没有排队切换关卡，服务端可能会停留在黑屏或无响应状态。
- **修复方案**：
  - 监听 `engine2` 中的关卡切换排队状态。若比赛已结束、下一个关卡目标为空且引擎原生未安排换图队列，在超时后自动重新加载当前地图以恢复循环。原生安排的换图流程具有更高优先级。

#### 11. 长图动画时钟刷新 (`sf_slow_animation`，默认 0)

- **引擎缺陷**：在长期不换图的死斗、挂机、KZ 或 Surf 服务器上，随着游戏时钟持续累积，单精度浮点数时间戳精度下降，导致骨骼动画、投掷物飞行或移动视觉出现严重顿挫与卡顿（Slow Animation Bug）。
- **修复方案**：
  - 空服地图默认运行 30 分钟且连续 5 分钟无真人后可刷新。有人时，引擎时钟达到 180 分钟后优先等下一场比赛；等待 120 分钟仍未开始新比赛时，提前 2 分钟通知并等回合边界，最多再等 10 分钟。原生换图优先；重载同一地图且时限基准未变时保留剩余时间。

#### 12. 空闲客户端槽位修复 (`sf_free_client_fix`，默认 0)

- **当前状态**：当前为不可用保留状态（默认值为 `0`）。
- **设计考量**：在尚未完全确定错误拒绝对应的客户端槽位在引擎内部可以安全复用且不会造成状态残留或崩溃之前，本插件不盲目强行复用槽位，以防引发深层内存破坏。`sf_status` 会如实报告 `no verified reusable client at native rejection; no hook installed`。

---

### 与 CS2Fixes 及其他插件的冲突防范

> [!WARNING]
> 不要将本插件的原生实体修复与 CS2Fixes 或 GameFixes 中相同功能的 Hook 同时启用！

1. **不可重叠挂载**：PolyHook 对同一机器码地址重复挂载 Detour 或使用不兼容的 ABI 约定可能导致递归调用、跳板损毁或崩溃。
2. **主动防冲突检测**：在安装 Hook 前，插件通过 `polyhook::FindDetour` 检测目标地址是否已被其他插件挂载。若已存在 Detour，插件主动放弃挂载并在 `sf_status` 中提示：
   `native entry already detoured; hook ABI/ownership cannot be shared`。
3. **磁盘二进制校验 (`ServerCodeUnmodified`)**：在安装任何 Detour 或 Mid-Hook 之前，插件会直接读取磁盘上的 `server.dll`、`engine2.dll` 或 Linux `.so` 二进制文件，比对内存中待 Hook 区域的指令字节（最多覆盖 23 字节的 PolyHook 写入窗口）。若内存指令与磁盘原始代码不一致（例如已被第三方补丁修改），插件立即安全放弃，防止在未知指令序列上错误挂载。

---

## English

`imomoe_server_fixes` uses Gamedata to locate native entry points for entity, movement, network message, and long-running map fixes. Enable only the modules you need and check availability with `sf_status`.

Each module fails closed safely: if signatures do not match or the target instructions have been modified by a third-party plugin, the module stays disabled and reports the diagnostic reason in `sf_status`.

### Module Details

#### 1. Unparented Beam Origin & Endpoint Hang Fix (`sf_beam_fix`, default 1)

- **Engine Defect**: Calling `SetOrigin` or `SetEndPos` on an unparented `CBeam` entity (laser or beam) causes the engine to traverse an assumed parent hierarchy during physics and scene-graph updates, leading to deadlocks, infinite loops, or server crashes.
- **Repair**:
  - Hooks `ServerFixes::BeamSetOrigin` and `ServerFixes::BeamSetEndPos`.
  - Queries the beam's parent pointer via `ServerFixes::BeamGetParent`. If a parent exists, returns `ResultType::Ignored` to allow native processing.
  - If unparented:
    - `SetOrigin`: Dispatches directly to the native local origin setter (`ServerFixes::BeamWriteLocalOrigin`), bypassing the broken hierarchy traversal;
    - `SetEndPos`: Compares current and target vectors. If changed, notifies the engine via `beam->NetworkStateChanged(m_vecEndPos)`, writes the target position directly, and returns `ResultType::Supercede`.

#### 2. Filter TestActivator Without Activator (`sf_filter_activator_fix`, default 1)

- **Engine Defect**: When map logic fires a `TestActivator` input to a `filter_*` entity without an activator (`nullptr`, e.g. from logic timers or relays), native checks mishandle the null entity and may fire false `OnPass` or `OnFail` outputs.
- **Repair**:
  - Hooks `ServerFixes::FilterAcceptInput`.
  - If the input name matches `TestActivator` (case-insensitive), the entity classname starts with `filter_`, and the activator pointer is `nullptr`:
  - Sets the return boolean to `true` (consumed) and returns `ResultType::Supercede`, suppressing the faulty check and preventing invalid outputs.

#### 3. Equipment Item Precaching Skip (`sf_equip_precache_fix`, default 1)

- **Engine Defect**: During map precaching, `game_player_equip` attempts to precache all listed item names. When item keys start with `item_*` (such as armor), the engine precache routine cannot resolve the classname properly, emitting warnings or errors.
- **Repair**:
  - Places a register mid-hook (`HookDetour2`) at `ServerFixes::PlayerEquipPrecacheLookup`.
  - Inspects register `RAX` containing the string lookup result. If the name starts with `item_` (case-insensitive), `RAX` is temporarily pointed to an empty string `""`.
  - The native logic cleanly skips precaching for empty strings, while preserving the entity's original KeyValues for runtime spawning and equipment distribution.

#### 4. Map Entity Hammer ID Preservation (`sf_hammer_id`, default 0)

- **Engine Defect**: On dedicated servers, native map entity parsing branches skip reading and storing `hammerUniqueId`, leaving entity Hammer IDs inaccessible to plugins or admin tools.
- **Repair**:
  - Uses `OwnedCodePatch` to patch the conditional skip branch (JZ) at `ServerFixes::HammerIdSkip` into a fallthrough.
  - Native entity properties and typing checks remain untouched; initialized entities properly retain their original Hammer IDs. Must be enabled before loading the target map.

#### 5. CSTV Bot Quota Exclusion (`sf_cstv_bot_quota`, default 0)

- **Engine Defect**: When `bot_quota` management runs alongside SourceTV/CSTV (`tv_enable 1`), the native candidate selection may evaluate the CSTV controller for removal, causing accidental disconnections or quota imbalances.
- **Repair**:
  - Intercepts `ServerFixes::SelectAndKickBot` and `ServerFixes::BotTeamCandidate`.
  - Checks the HLTV controller flag (`controller[g_hltvOffset] != 0`) and excludes CSTV from removal candidates; leaves bot quotas and normal disconnects untouched.

#### 6. Water Jump Momentum Fix (`sf_water_jump`, default 0)

- **Engine Defect**: When moving out of water (`WaterMove`), the engine immediately calls `CategorizePosition(..., stayOnGround=false)`. CS2 incorrectly classifies the player as touching ground here, resetting vertical velocity and wiping jump momentum.
- **Repair**:
  - Hooks `ServerFixes::WaterMove` and `ServerFixes::CategorizePosition`.
  - Skips the ground classification once (`Supercede`) when it immediately follows a water move in the same movement context; normal ground classification resumes immediately afterward.

#### 7. One Jump Per Movement Tick (`sf_jump_spam`, default 0)

- **Engine Defect**: Applying multiple jumps in one movement tick can produce inconsistent movement.
- **Repair**:
  - Hooks `ServerFixes::LegacyJumpAttempt`, `ServerFixes::ModernJumpAttempt`, and `ServerFixes::CompleteJump`.
  - Tracks completed native jumps per pawn per tick. If a pawn has already completed a jump in the current tick, subsequent attempts in that tick are suppressed; failed attempts do not consume quota, preserving normal bunnyhopping.

#### 8. Slope Seam Collision & Rampbug Fix (`sf_rampbug`, default 0)

- **Engine Defect**: On surf, KZ, or slope maps, polygon seams with subtle normal inconsistencies can catch sliding players, causing abrupt stops or vertical damage (Rampbug).
- **Repair**:
  - Hooks `ServerFixes::RampTryPlayerMove`, `ServerFixes::RampCachedHullTrace`, `ServerFixes::RampUncachedHullTrace`, and `CategorizePosition`.
  - When sliding across a seam with an inconsistent trace normal, nudges the trace along the tracked ramp normal to find a clear path (`FindCorrection`). Restores landing horizontal velocity when landing on slope surfaces. Does not interfere with flat ground, water, or noclip movement.

#### 9. KeyValues Command Flood Protection (`sf_keyvalues_flood`, default 0)

- **Purpose**: Bound the cost of processing KeyValues commands per connection.
- **Repair**:
  - Detours `CServerSideClient::ProcessCmdKeyValues` in `engine2`.
  - Enforces a token-bucket rate limit per connection (default 10/s, burst 20, max 1500 bytes). Excess or oversized messages are dropped cleanly without disconnecting the client.

#### 10. Blank Map Recovery (`sf_blank_map`, default 0)

- **Engine Defect**: If match end yields an empty next-level target and the engine queues no transition, dedicated servers may remain indefinitely frozen on an empty screen.
- **Repair**:
  - Monitors queued transitions in `engine2`. If the match has ended with no target and no transition is pending, reloads the current map after a brief timeout. Native transition queues always take precedence.

#### 11. Long-Running Map Clock Refresh (`sf_slow_animation`, default 0)

- **Engine Defect**: On servers with very long uptimes (e.g. idle, KZ, surf), floating-point time degradation in the engine clock causes visual animation stutter and jitter (Slow Animation Bug).
- **Repair**:
  - By default, an empty map can refresh after 30 minutes on the map and five minutes without human players. With players present, the engine clock reaching 180 minutes starts a wait for the next match. After 120 minutes without a new match, the server warns players two minutes ahead and waits up to 10 more minutes for a round boundary. Native map changes take priority. Reloading the same map keeps its remaining time when the time-limit baseline is unchanged.

#### 12. Free Client Slot Reuse (`sf_free_client_fix`, default 0)

- **Status**: Reserved and disabled by default (`0`).
- **Rationale**: Reusing client slots rejected by the engine risks corrupting connection state unless the slot is proven completely clean and recyclable. `sf_status` reports `no verified reusable client at native rejection; no hook installed`.

---

### Conflict Avoidance with CS2Fixes and GameFixes

> [!WARNING]
> Do not enable overlapping entity or movement hooks in both `imomoe_server_fixes` and CS2Fixes/GameFixes simultaneously.

1. **No Shared Detours**: Overlapping PolyHook detours on the same target address cause undefined behavior or crashes.
2. **Detection & Fail-Closed**: Before attaching a hook, the plugin checks `polyhook::FindDetour`. If an existing detour is found, it refuses to attach and reports `native entry already detoured; hook ABI/ownership cannot be shared` in `sf_status`.
3. **On-Disk Binary Verification (`ServerCodeUnmodified`)**: Before hooking, the plugin reads the on-disk binary (`server.dll`, `engine2.dll`, or Linux shared libraries) and compares up to 23 bytes of executable code at the hook address against the live memory image. If any divergence is detected, hook installation is aborted.
