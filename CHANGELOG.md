# Changelog / 更新日志

All notable changes to this project are documented in this file.
本项目的所有重要变更均记录在此文件中。

---

## [0.1.0] - Initial Standalone Release / 首个独立版本

### 简体中文

- **独立插件架构 (`imomoe_server_fixes`)**：将服务器底层修复能力从 FunRounds 中解耦为独立的 C++ Plugify 插件，通过导出 `GetOwnedFixes()` 掩码（`0x3`）与支持桥接的 FunRounds 协同工作，也可单独部署。
- **聊天准入拦截 (`chat_admission`)**：拦截尚未完全进入游戏（Pre-InGame）的客户端通过 `say`、`say_team` 或聊天别名通道发送的聊天指令，不影响 RCON、服务端命令及非聊天指令。
- **人质救援与背负兼容 (`hostage_compat`)**：
  - **CT 救援触发补偿**：针对自定义玩法或高负载下玩家接触救援区（`Touch`）已完成派发但原生 `CHostage_API::OnRescueZoneTouch` 未被调用的情况，在核验地图、回合、存活状态及实体句柄后执行一次性原生救援输入补偿。
  - **T 阵营背负人质**：当 `mp_only_cts_rescue_hostages` 设为 `0` 时，放开原生人质交互目标选择器的阵营检查，允许 T 阵营背起人质；禁用模块或切回 `1` 时自动还原内存补丁。
- **出生点容量扩展 (`spawn_capacity`)**：
  - 在 `de_poseidon` 上基于原生有效出生点自动为 T / CT 补足出生点实体至服务器实际 `maxplayers`，并按需同步阵营上限。
  - 当客户端因出生点容量刚完成扩展而在首次 `jointeam 2/3` 时被拒，支持安全重放一次加队请求。
  - 提供服务端控制台手动出生点工具（`sf_spawn_add`、`sf_spawn_list`、`sf_spawn_remove`、`sf_spawn_save`），支持跨地图重载持久化。
- **观察者容量同步 (`spectator_compat`)**：自动将 `mp_spectators_max` 与 `CCSGameRules::m_iSpectatorSlotCount` 对齐至服务器实际 `maxplayers`；若外部插件或管理员在运行期主动修改了这两个值，本模块会自动让出控制权并在卸载或换图时仅还原自身持有的改动。
- **状态查询与配置迁移**：新增 `sf_status` 控制台命令及 `tools/migrate_config.py` 配置迁移脚本。

### English

- **Standalone Plugin Architecture (`imomoe_server_fixes`)**: Extracted server-level fixes from FunRounds into a standalone C++ Plugify plugin. Exports `GetOwnedFixes()` (`0x3`) to coordinate cleanly with bridge-aware FunRounds builds while remaining usable on its own.
- **Chat Admission (`chat_admission`)**: Blocks `say`, `say_team`, and SDK chat-alias commands from client slots that have not yet reached the `InGame` state, leaving RCON, server commands, and non-chat client commands untouched.
- **Hostage Compatibility (`hostage_compat`)**:
  - **CT Rescue Compensation**: Observes `CBaseEntity::Touch`, the pawn rescue dispatch site, and `CHostage_API::OnRescueZoneTouch`. When a valid CT carrying a hostage reaches the rescue dispatch path without entering the native hostage input handler, submits a single verified `OnRescueZoneTouch` input.
  - **T Hostage Pickup**: When `mp_only_cts_rescue_hostages` is `false`, patches the native hostage target selector's team check so Terrorists can pick up hostages, and cleanly restores the original bytes when disabled.
- **Spawn Capacity (`spawn_capacity`)**:
  - Automatically supplements `info_player_terrorist` and `info_player_counterterrorist` entities on `de_poseidon` up to the server's actual `maxplayers`, adjusting per-team maximums when needed.
  - Replays a single rejected `jointeam 2/3` command via `IServerGameClients::ClientCommand` when capacity was just expanded.
  - Provides server console commands (`sf_spawn_add`, `sf_spawn_list`, `sf_spawn_remove`, `sf_spawn_save`) to manage persistent manual spawn points per map.
- **Spectator Capacity (`spectator_compat`)**: Synchronizes `mp_spectators_max` and `CCSGameRules::m_iSpectatorSlotCount` with the server's actual `maxplayers`. Yields ownership if an external writer modifies either property during an active cycle and restores only plugin-owned values on disable, map end, or unload.
- **Diagnostics & Migration**: Added the `sf_status` console command and `tools/migrate_config.py` for migrating legacy FunRounds TOML settings.
