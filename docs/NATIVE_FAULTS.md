# Native Fault Fixes / 原生实体修复说明

[简体中文](#简体中文) | [English](#english)

---

## 简体中文

CS2 引擎在处理特定实体输入、预缓存或无父级几何对象时存在已知缺陷。`imomoe_server_fixes` 提供了一组原生底层修复模块（`beam_fix`、`filter_activator_fix`、`equip_precache_fix` 及保留项 `free_client_fix`），通过 Gamedata 签名定位与 PolyHook Detour 进行外科手术式拦截与安全补偿。

### 模块机制详解

#### 1. 光束坐标卡死修复 (`sf_beam_fix`)

- **引擎缺陷**：当地图或脚本对无父级（Unparented）的 `CBeam`（光束、激光）实体调用坐标设置函数（`SetOrigin` 或 `SetEndPos`）时，原生逻辑在更新物理层级与场景图时假设实体具备父级层级，可能导致服务端死锁、无限循环或崩溃。
- **修复方案**：
  - 拦截 `ServerFixes::BeamSetOrigin` 与 `ServerFixes::BeamSetEndPos`。
  - 首先通过 `ServerFixes::BeamGetParent` 查询实体的父级指针。若实体存在父级，直接忽略（`ResultType::Ignored`），保留引擎原生处理链路。
  - 若实体无父级：
    - `SetOrigin`：直接调用原生本地坐标写入函数（`ServerFixes::BeamWriteLocalOrigin`），绕过错误的父级层级分发；
    - `SetEndPos`：对比当前坐标与目标坐标，若发生改变，调用 `beam->NetworkStateChanged(m_vecEndPos)` 触发网络状态同步，并直接写入目标坐标，返回 `ResultType::Supercede` 阻止原生问题逻辑。

#### 2. 过滤器无触发者修复 (`sf_filter_activator_fix`)

- **引擎缺陷**：当实体 I/O 触发器向 `filter_*` 实体发送 `TestActivator` 输入但未传递有效 Activator（指针为空，例如由计时器或中继触发）时，原生判定逻辑存在空指针假设缺陷，可能触发错误的 `OnPass` 或 `OnFail` 输出，导致地图逻辑流程混乱。
- **修复方案**：
  - 拦截 `ServerFixes::FilterAcceptInput`。
  - 检查输入名称是否为 `TestActivator`（大小写不敏感），实体类名是否以 `filter_` 开头，且 Activator 指针是否为 `nullptr`。
  - 命中上述条件时，直接将返回值设为 `true`（表示输入已消费）并返回 `ResultType::Supercede`，阻断原生处理链路，既不执行测试逻辑，也不误触发任何通过/失败输出。

#### 3. 装备预缓存跳过 (`sf_equip_precache_fix`)

- **引擎缺陷**：在地图加载和实体预缓存阶段，`game_player_equip` 会枚举待发放的物品类名并尝试预缓存。当类名包含 `item_*`（如护甲等）时，原生物品预缓存机制无法正确解析该类标识，导致非预期的引擎告警或加载异常。
- **修复方案**：
  - 在 `ServerFixes::PlayerEquipPrecacheLookup` 安装寄存器 Mid-Hook（`HookDetour2`）。
  - 监控读取物品名称结果的 `RAX` 寄存器。若物品名称以 `item_` 开头（大小写不敏感），将 `RAX` 临时替换为指向空字符串 `""` 的指针。
  - 原生代码在遇到空字符串时会安全跳过本次预缓存调用；实体的原始 KeyValues 与后续正式生成、装备发放逻辑完全不受影响。

#### 4. 空闲客户端槽位修复 (`sf_free_client_fix`)

- **当前状态**：当前为不可用保留状态（默认值为 `0`）。
- **设计考量**：在尚未完全确定错误拒绝对应的客户端槽位在引擎内部可以安全复用且不会造成状态残留或崩溃之前，本插件不盲目强行复用槽位，以防引发深层内存破坏。`sf_status` 会如实报告 `no verified reusable client at native rejection; no hook installed`。

---

### 与 CS2Fixes 及其他插件的冲突防范

> [!WARNING]
> 不要将本插件的原生实体修复与 CS2Fixes 中相同功能的 Detour 同时启用！

1. **不可重叠挂载**：`polyhook` 对同一机器码地址重复挂载 Detour 或使用不兼容的 ABI 约定可能导致递归调用、跳板损毁或崩溃。
2. **主动防冲突检测**：在安装 Hook 前，插件通过 `polyhook::FindDetour` 检测目标地址是否已被其他插件挂载。若已存在 Detour，插件主动放弃挂载并在 `sf_status` 中提示：
   `native entry already detoured; hook ABI/ownership cannot be shared`。
3. **磁盘二进制校验 (`ServerCodeUnmodified`)**：在安装任何 Detour 或 Mid-Hook 之前，插件会直接读取磁盘上的 `server.dll` 或 `libserver.so` 二进制文件，比对内存中待 Hook 区域的指令字节（最多覆盖 23 字节的 PolyHook 写入窗口）。若内存指令与磁盘原始代码不一致（例如已被第三方补丁修改），插件立即安全放弃，防止在未知指令序列上错误挂载。

---

## English

The Counter-Strike 2 engine contains known defects when handling certain entity inputs, precaching routines, or unparented geometry objects. `imomoe_server_fixes` provides low-level native fixes (`beam_fix`, `filter_activator_fix`, `equip_precache_fix`, and the reserved `free_client_fix`) through Gamedata signatures and surgical PolyHook detours.

### Module Details

#### 1. Unparented Beam Origin & Endpoint Hang Fix (`sf_beam_fix`)

- **Engine Defect**: Calling `SetOrigin` or `SetEndPos` on an unparented `CBeam` entity (laser or beam) causes the engine to traverse an assumed parent hierarchy during physics and scene-graph updates, which leads to deadlocks, infinite loops, or server crashes.
- **Repair**:
  - Hooks `ServerFixes::BeamSetOrigin` and `ServerFixes::BeamSetEndPos`.
  - Queries the beam's parent pointer via `ServerFixes::BeamGetParent`. If a parent exists, the hook returns `ResultType::Ignored` to allow native processing.
  - If unparented:
    - `SetOrigin`: Dispatches directly to the native local origin setter (`ServerFixes::BeamWriteLocalOrigin`), treating local coordinates as world origin and bypassing the broken hierarchy traversal;
    - `SetEndPos`: Compares current and target vectors. If changed, notifies the engine via `beam->NetworkStateChanged(m_vecEndPos)`, writes the target position directly, and returns `ResultType::Supercede`.

#### 2. Filter TestActivator Without Activator (`sf_filter_activator_fix`)

- **Engine Defect**: When map I/O sends a `TestActivator` input to a `filter_*` entity without an activator (a null pointer, such as from logic timers or relays), native checks mishandle the null entity and may fire false `OnPass` or `OnFail` outputs, breaking map logic.
- **Repair**:
  - Hooks `ServerFixes::FilterAcceptInput`.
  - If the input name matches `TestActivator` (case-insensitive), the entity classname starts with `filter_`, and the activator pointer is `nullptr`:
  - Sets the return boolean to `true` (input consumed) and returns `ResultType::Supercede`, suppressing the faulty check and preventing invalid outputs.

#### 3. Equipment Item Precaching Skip (`sf_equip_precache_fix`)

- **Engine Defect**: During map precaching, `game_player_equip` attempts to precache all listed item names. When item keys start with `item_*` (such as armor), the engine precache routine cannot resolve the classname properly, emitting warnings or errors.
- **Repair**:
  - Places a register mid-hook (`HookDetour2`) at `ServerFixes::PlayerEquipPrecacheLookup`.
  - Inspects register `RAX` containing the string lookup result. If the name starts with `item_` (case-insensitive), `RAX` is temporarily pointed to an empty string `""`.
  - The native logic cleanly skips precaching for empty strings, while preserving the entity's original KeyValues for runtime spawning and equipment distribution.

#### 4. Free Client Slot Reuse (`sf_free_client_fix`)

- **Status**: Reserved and disabled by default (`0`).
- **Rationale**: Reusing client slots rejected by the engine risks corrupting connection state unless the slot is proven completely clean and recyclable. `sf_status` reports `no verified reusable client at native rejection; no hook installed`.

---

### Conflict Avoidance with CS2Fixes

> [!WARNING]
> Do not enable overlapping entity fixes in both `imomoe_server_fixes` and CS2Fixes simultaneously.

1. **No Shared Detours**: Overlapping PolyHook detours on the same target function cause undefined behavior or crashes.
2. **Detection & Fail-Closed**: Before attaching a hook, the plugin checks `polyhook::FindDetour`. If an existing detour is found, it refuses to attach and reports `native entry already detoured; hook ABI/ownership cannot be shared` in `sf_status`.
3. **On-Disk Binary Verification (`ServerCodeUnmodified`)**: Before hooking, the plugin reads the on-disk binary (`server.dll` on Windows, `libserver.so` on Linux) and compares up to 23 bytes of executable code at the hook address against the live memory image. If any divergence is detected, hook installation is aborted.
