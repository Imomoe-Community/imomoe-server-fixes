# Contributing Guide / 贡献指南

感谢你对 **Imomoe Server Fixes** 的关注与贡献！为保持代码库清晰、可靠且易于维护，请在提交代码或 Issue 前阅读以下说明。

Thank you for contributing to **Imomoe Server Fixes**! Please review the guidelines below before opening an issue or pull request.

---

## 1. 职责边界 / Scope & Ownership

- **专注服务器底层修复 / Server-level fixes only**：本仓库仅负责服务器层面的底层修复与容量兼容（聊天准入 `chat_admission`、人质兼容 `hostage_compat`、出生点容量 `spawn_capacity`、观察者容量 `spectator_compat`）。娱乐回合玩法逻辑请提交至 `FunRounds` 仓库。
  This repository owns server-level fixes and capacity compatibility (`chat_admission`, `hostage_compat`, `spawn_capacity`, `spectator_compat`). Keep gameplay features in `FunRounds`.
- **安全与所有权原则 / Ownership & Safety**：
  - 涉及引擎实体、Schema 或内存补丁的操作必须在游戏主线程（Game Thread）执行。
    Engine entity, Schema, and memory patch operations must run on the game thread.
  - 延迟动作执行前必须核验地图代次（Map Generation）、回合、玩家存活状态及完整实体句柄。
    Always revalidate map generation, round, life state, and full entity handles before executing delayed actions.
  - 仅还原由本插件写入且当前仍由本插件持有的配置或属性；若外部插件或管理员修改了某项值，应主动让出控制权。
    Restore only values still owned by this plugin; yield cleanly if an external writer modifies a managed ConVar or GameRules field.

---

## 2. 本地测试（无需 SDK）/ Quick Local Checks (No SDK Required)

纯策略逻辑（`src/*_policy.hpp`、`src/chat_listener.hpp`、`src/spawn_capacity_storage.hpp`）与配置迁移脚本不依赖 Source 2 SDK，可直接通过 CMake Preset 和 Python `unittest` 运行验证：

Policy headers and the configuration migration tool do not require the Source 2 SDK and can be tested directly:

```sh
cmake --preset policy
cmake --build --preset policy --parallel 2
ctest --preset policy
python -m unittest discover -s tests -p "test_*.py"
```

> **提示 / Tip**：个人的本地生成器或 SDK 路径请写入 `CMakeUserPresets.json`（已在 `.gitignore` 中忽略），不要提交到仓库。
> Put personal generator preferences or local SDK paths in `CMakeUserPresets.json` (ignored by Git).

---

## 3. 完整插件构建与导出绑定 / Full Plugin Build & Bindings

- 当修改涉及运行时回调、S2SDK 调用、SourceSDK 接口或 `gamedata/imomoe_server_fixes.jsonc` 时，请使用 [依赖说明](docs/DEPENDENCIES.md) 与 [README 构建指南](README.md) 中指定的配对 S2SDK / SourceSDK 工作树完成完整构建与回调测试（`-DSERVER_FIXES_CALLBACK_TESTS=ON`）。
  When changing runtime callbacks, S2SDK/SourceSDK calls, or `gamedata/imomoe_server_fixes.jsonc`, build against the paired S2SDK/SourceSDK worktree described in [docs/DEPENDENCIES.md](docs/DEPENDENCIES.md) and run callback tests (`-DSERVER_FIXES_CALLBACK_TESTS=ON`).
- `generated-s2sdk/` 目录需要随仓库检入，且仅允许从本仓库的 `bindings/s2sdk.pplugin` 生成，请勿使用 S2SDK 根目录的 `.pplugin.in` 模板覆盖。
  Keep `generated-s2sdk/` checked into Git and regenerate it only from `bindings/s2sdk.pplugin`.

---

## 4. 代码与文档风格 / Code & Documentation Style

- **代码格式 / Formatting**：C++ 代码使用 Tab 缩进（宽度为 4），JSON / YAML 使用 2 空格缩进，统一使用 LF 换行符（详见 `.editorconfig` 与 `.clang-format`）。
  C++ files use tabs (width 4), JSON/YAML files use 2 spaces, and all files use LF line endings (see `.editorconfig` and `.clang-format`).
- **文档语言 / Documentation**：项目文档请保持中英双语、结构清晰、面向真人读者编写，避免晦涩冗长的机器翻译腔。
  Keep project documentation bilingual (Simplified Chinese & English), concise, and easy for humans to read.
