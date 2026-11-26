<div align="center">

# Imomoe Server Fixes (`imomoe_server_fixes`)

[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](LICENSE)
[![C++: C++23](https://img.shields.io/badge/C%2B%2B-C%2B%2B23-orange.svg)](CMakeLists.txt)
[![CS2: Compatible](https://img.shields.io/badge/CS2-Dedicated%20Server-black.svg)](https://www.counter-strike.net/)
[![Platform: Plugify](https://img.shields.io/badge/Plugify-v3.1.0%2B-purple.svg)](https://github.com/untrustedmodders/plugify)
[![CI](https://github.com/Imomoe-Community/imomoe-server-fixes/actions/workflows/ci.yml/badge.svg)](https://github.com/Imomoe-Community/imomoe-server-fixes/actions/workflows/ci.yml)

**A high-performance, lightweight Counter-Strike 2 dedicated server fixes and capacity compatibility extension.**

[双语 / Bilingual](README.md) · **English**

</div>

---

### Overview

**imomoe_server_fixes** is an independent native C++ [Plugify](https://github.com/untrustedmodders/plugify) plugin for Counter-Strike 2 dedicated servers. It resolves low-level engine defects, crash exploits, early chat admission, hostage rescue compatibility, spawn capacity auto-expansion, and spectator slot synchronization.

The plugin has zero runtime dependency on `FunRounds` or `funrounds_native`. It runs standalone as a server maintenance suite, and cooperates with bridge-aware `FunRounds` builds (`GetOwnedFixes()`) and optional `imomoe_admin` (`>= 0.1.0`, noclip ownership).

---

### Key Features

| Category | Description | Default |
| :--- | :--- | :--- |
| **Chat Admission** | Blocks `say`, `say_team`, and SDK chat aliases from clients that have not reached `InGame` status. Server/RCON commands and connected players are unaffected. | Enabled (`sf_chat_admission 1`) |
| **Hostage Compatibility** | 1. **CT rescue compensation**: monitors hostage rescue touch dispatches and resubmits missing `OnRescueZoneTouch` inputs after validating liveness and handles.<br>2. **T hostage pickup**: conditionally patches hostage target selection when `mp_only_cts_rescue_hostages 0`, and restores original bytes when set to `1` or disabled. | Enabled (`sf_hostage_compat 1`) |
| **Spawn Capacity** | Automatically supplements spawn points for both teams up to `maxplayers` on all maps, retries team joins on expansion, and provides manual server console spawn editor commands. | Enabled (`sf_spawn_capacity 1`) |
| **Spectator Capacity** | Synchronizes `mp_spectators_max` and `CCSGameRules::m_iSpectatorSlotCount` to `maxplayers` while safely yielding ownership on manual override, map change, or unload. | Enabled (`sf_spectator_compat 1`) |
| **Native Fault Fixes** | Beam, filter activator, and equipment precache fixes are enabled by default. Hammer ID, CSTV quota selection, water jumping, per-tick jump limits, enemy movement collision following teammate rules, and KeyValues rate limiting are opt-in. See [Native Faults Documentation](docs/NATIVE_FAULTS.md#english). | Core fixes active |
| **Opt-in Compatibility Guards** | Voice validation and rate limits, team-switch rate limits, noclip reset when cheats are disabled, Workshop map speaker streams, and spectator pawn reconciliation. | Opt-in (disabled by default) |
| **Map Maintenance & Ramp** | 1. `sf_blank_map`: reloads the current map if a match ends without a queued destination.<br>2. `sf_slow_animation`: schedules long-running map refreshes based on elapsed time and player activity.<br>3. `sf_rampbug`: retraces inconsistent ramp seams and preserves horizontal speed on slope landings. | Opt-in (disabled by default) |
| **Game Ban Fix** | Verifies current authenticated Steam identity, cleans expired GC bans, and avoids pending SteamID kick cycles while strictly retaining active game bans. | Opt-in (`sf_game_ban_fix 0`) |

<details>
<summary><b>Click to expand Native Fault Fixes details</b></summary>

- **Unparented Beam Fix (`sf_beam_fix`)**: Prevents engine deadlocks when setting origin or endpoint coordinates on unparented `CBeam` entities.
- **Filter Null Activator Fix (`sf_filter_activator_fix`)**: Suppresses improper filter pass/fail firing when `TestActivator` input lacks a valid activator.
- **Equipment Precache Skip (`sf_equip_precache_fix`)**: Bypasses erroneous `item_*` precache lookups during `game_player_equip` map precache without altering equipment grants.
- **Hammer ID Retention (`sf_hammer_id`)**: Retains original `hammerUniqueId` values across entity spawn passes.
- **CSTV Bot Quota Exclusion (`sf_cstv_bot_quota`)**: Exempts CSTV clients when balancing bot quota removals.
- **Water Jump Fix (`sf_water_jump`)**: Skips anomalous single-tick ground reclassification when exiting water bodies.
- **One Jump Per Tick (`sf_jump_spam`)**: Limits physical jump execution to once per tick per entity while preserving normal bunnyhopping.
- **KeyValues Flood Protection (`sf_keyvalues_flood`)**: Applies token-bucket rate limiting against KeyValues command floods without disconnecting clients.

See [Native Faults Documentation](docs/NATIVE_FAULTS.md#english) for full root-cause analyses and verification steps.

</details>

<details>
<summary><b>Click to expand Architecture & Reliability</b></summary>

- **On-Disk Binary Verification (`ServerCodeUnmodified`)**: Verifies disk image instruction bytes (`server.dll`/`engine2.dll` or `.so`) before detouring. Automatically aborts on external hooks (e.g. CS2Fixes) and reports in `sf_status` to prevent double-hook crashes.
- **Owned Code Patches (`OwnedCodePatch`)**: Hostage pickup and Hammer ID share patch and page-protection handling; only bytes still owned by this plugin are restored on disable or unload.
- **Multi-Image Resolution (`LocateServerImage` / `LocateEngineImage`)**: Resolves both server and `engine2` libraries with memory boundary verification.
- **FunRounds Ownership Bridge**: Exports `GetOwnedFixes()` returning fixed capability mask `3` (bit 0: `chat_admission`, bit 1: `hostage_compat`).

</details>

---

### Installation

#### Prerequisites

- **Dedicated Server**: Counter-Strike 2 Dedicated Server (Windows x64 / Linux x64)
- **Plugify Host**: `>= 3.1.0`
- **S2SDK**: Requires S2SDK `>= 3.4.3`; the [paired build uses official 3.5.1](docs/DEPENDENCIES.md).
- **PolyHook**: Minimum `>= 2.5.9`; Windows and Linux were validated with the official [`2.5.9`](https://github.com/untrustedmodders/plugify-plugin-polyhook/releases/tag/v2.5.9) release.

#### Deployment Structure

Deploy plugin artifacts to the dedicated server root directory:

```text
game/csgo/
└── addons/plugify/envs/imomoe_server_fixes/
    ├── bin/
    │   └── imomoe-server-fixes.dll                 # Linux: libimomoe-server-fixes.so
    ├── gamedata/
    │   └── imomoe_server_fixes.jsonc               # Gamedata signatures and offsets
    └── imomoe_server_fixes.pplugin                 # Manifest
```

The plugin loads `game/csgo/cfg/imomoe/imomoe_server_fixes.cfg` at startup and when switching to a different map. Reloading the same map via `changelevel` does not run it again (unloading and reloading the plugin runs startup again). Defaults are generated only if the file is missing; no `server.cfg` entry is needed.

**Upgrading from the old path:** Before starting the upgraded plugin, transfer the settings you want to keep to the new file, retain the old `cfg/imomoe_server_fixes.cfg` as a backup, and remove the old `exec` from `server.cfg`, launch arguments, or deployment scripts. The plugin does not read or move the old file.

---

### Commands Reference

| Command | Arguments | Description |
| :--- | :--- | :--- |
| `sf_status` | None | Displays enabled state, runtime availability, and hook/patch diagnostics for all modules. |
| `sf_spawn_add` | `<T\|CT> <x y z> [pitch yaw roll]` | Adds a manual spawn point for T or CT at specified world coordinates. |
| `sf_spawn_list` | None | Lists all active manual spawn points for the current map. |
| `sf_spawn_remove` | `<id>` | Removes a specific manual spawn point without touching native spawns. |
| `sf_spawn_save` | None | Saves manual spawn points to `cfg/imomoe_server_fixes/spawn_capacity/<map>.cfg` for future map loads. |

In `sf_status`, `hostage_compat` `CT` reports rescue-zone observer readiness and any map or game-mode restriction. `T` is evaluated independently, so `available=1` may reflect only T-side availability. `CT=ready` describes the observer; check native events for an individual rescue result.

---

### Configuration

See the [configuration template](configs/imomoe_server_fixes.cfg) for defaults and options. Edit `game/csgo/cfg/imomoe/imomoe_server_fixes.cfg` and check `sf_status` after the next map load. To apply changes now, run `exec imomoe/imomoe_server_fixes.cfg` from the server console.

| Use case | Setting |
| :--- | :--- |
| Check that voice messages contain audio | `sf_voice_fix 1` |
| Rate-limit voice messages per client | `sf_voice_max_per_second 128` |
| Limit team and spectator switches per client over 10 seconds | `sf_team_switch_limit 15` |
| Restore walking when cheats are disabled; clear retained noclip across respawns | `sf_noclip_reset 1` |
| Separate speaker streams on Workshop maps | `sf_workshop_voice 1` |

`sf_noclip_reset` preserves noclip only for the exact current slot and pawn handle claimed by optional `imomoe_admin` (`>= 0.1.0`). The exemption does not carry over to a new pawn; cleanup works without Admin.

A limit of `0` disables that rate guard. Excess messages are dropped without disconnecting the client. See [Native Fault Fixes](docs/NATIVE_FAULTS.md#english) for map maintenance and movement settings.

<details>
<summary><b>Configuration Migration</b></summary>

Migrate legacy FunRounds TOML settings (`hostage_rescue_compat`) to `.cfg`:

```powershell
python tools/migrate_config.py <path-to-funrounds.toml> --output hostage-setting.cfg
```

Transfer the generated setting to the new CFG without replacing its other contents.

</details>

---

### Building & Testing

#### 1. Policy & Unit Tests (No SDK Runtime Required)

Tests 8 independent policy suites (`chat_policy`, `chat_listener`, `hostage_policy`, `spawn_capacity_policy`, `spectator_compat_policy`, `compat_guard_policy`, `map_maintenance_policy`, `ramp_policy`) and Python migration/build tools:

```sh
cmake --preset policy
cmake --build --preset policy --parallel
ctest --preset policy

python -m unittest discover -s tests -p "test_*.py"
python -m unittest discover -s tools/tests -p "test_*.py"
```

#### 2. Full Plugin Build (With Paired SDK)

Production builds require C++23 and the paired S2SDK worktree specified in [DEPENDENCIES.md](docs/DEPENDENCIES.md):

```powershell
# Windows build example (PowerShell)
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

#### 3. Paired Release Packaging

Pass the official S2SDK source and its paired build. On Windows, use a Visual Studio x64 generator to build S2SDK. On Linux, build S2SDK at the same absolute paths inside a Steam Runtime GCC 14 container and pass that image to the packager.

```powershell
# Windows
python tools/build_paired.py --sdk-source <s2sdk-source> --sdk-build <s2sdk-build> --cs2-bin <game/bin/win64>

# Linux (with Docker)
python3 tools/build_paired.py --sdk-source <s2sdk-source> --sdk-build <s2sdk-build> --steamrt-image <image> --cs2-bin <game/bin/linuxsteamrt64>
```

Artifacts are generated into `dist/imomoe-server-fixes-<platform>-<name>.zip`.

Set `IMOMOE_WORK_ROOT` to put the build session in its `sessions/serverfixes-<name>` directory.
With `--funrounds-source`, use a FunRounds SDK builder that supports shared work roots; its session goes in `sessions/serverfixes-sdk-<name>`.
If the variable is unset or blank, the sessions stay in the respective ServerFixes and FunRounds `.work/` directories.

- [Dependencies](docs/DEPENDENCIES.md) · [Native Faults](docs/NATIVE_FAULTS.md#english) · [Contributing](CONTRIBUTING.md) · [Security](SECURITY.md) · [Changelog](CHANGELOG.md)

---

### License & Author

- **Author**: Mai Ooizumi ([@Mai-Ooizumi](https://github.com/Mai-Ooizumi))
- **Reference**: [ViTaminza/plugify-game_fixes](https://github.com/ViTaminza/plugify-game_fixes) (behavior reference for compatibility guards and native fixes)
- **License**: GNU Affero General Public License v3.0 or later ([AGPL-3.0-or-later](LICENSE))
