# Dependency Inputs / 依赖与构建输入说明

## 配对 SDK 构建输入 / Paired SDK Build

编译完整的 `imomoe_server_fixes` 插件时，请使用下表所列的固定版本与同一工作树下的配对构建产物（[`imomoe_server_fixes.pplugin`](../imomoe_server_fixes.pplugin) 中声明的最低运行版本仅作为加载兼容下限，不能替代编译期的配对 SDK 要求）：

When building the full `imomoe_server_fixes` plugin, use the pinned versions below from a single configured SDK worktree (the minimum version constraints in [`imomoe_server_fixes.pplugin`](../imomoe_server_fixes.pplugin) are runtime compatibility floors, not a substitute for these paired build inputs):

| 组件 / Input | 验证版本或来源 / Validated Version or Source |
| --- | --- |
| **S2SDK** | `3.3.1`, [signed fork commit `2fca1d1`](https://github.com/Mai-Ooizumi/plugify-plugin-s2sdk/tree/2fca1d177ddca1c265ee585457ad160752e9111c) |
| **SourceSDK** | S2SDK 子模块指针 [`f3de33c`](https://github.com/Wend4r/sourcesdk/tree/f3de33c58595c628c12b4ffaba76dfcd49b8763f) |
| **Plugify Host** | `3.1.0` |
| **Plugify C++ Language Module** | `2.4.5` |
| **PolyHook** | [`2.5.9`](https://github.com/untrustedmodders/plugify-plugin-polyhook/releases/tag/v2.5.9)（当前双端配对版本 / current Windows/Linux pair） |
| **plugify-gen** | `1.2.15`（仅在需要从 `bindings/s2sdk.pplugin` 重新生成绑定时使用 / for intentional binding regeneration） |

请检出上述固定提交的 S2SDK 并递归初始化其子模块（`git submodule update --init --recursive`）。编译本插件所需的 SourceSDK 公共头文件、生成的 Protobuf 头文件（`external/sourcesdk/protos`）、静态库 `tier0_perproject` 以及运行时 `tier0` 导入库均来自该 S2SDK 工作树及其已完成构建的 `build` 目录。具体 CMake 参数请参考 [README 构建指南](../README.md#build-the-plugin-with-the-paired-sdk)。

Check out S2SDK at the fixed commit above with recursive submodules initialized. The SourceSDK headers, generated protobuf headers (`external/sourcesdk/protos`), static `tier0_perproject` archive, and runtime `tier0` import library must all come from that same configured S2SDK build tree. See the [README build instructions](../README.md#build-the-plugin-with-the-paired-sdk) for the exact CMake arguments.

---

## Linux PolyHook 2.5.7 fallback

### 简体中文

若 Linux 环境仍使用旧版 `2.5.7`，必须包含针对 `mem_protect` 的跨页长度取整修复：在向上对齐受保护内存区域长度时，需要将起始地址自身的页内偏移（Page Offset）一并计入。

- **插件源码基准提交**：`5892ea6a0ce7e8c48748e054e91bbf4df973cc9f`
- **PolyHook_2 子模块提交**：`298d56210b9d9e66cde8f96481d6053925c6ae15`
- **上游修复 PR**：[untrustedmodders/plugify-plugin-polyhook#37](https://github.com/untrustedmodders/plugify-plugin-polyhook/pull/37)（合并提交 `0aadc50fb66afd5c77af4f601e5e4b25e1be7b86`）

未经此修复的原始 `2.5.7` Linux 二进制在处理跨越内存页边界的跳板（Trampoline）时，可能会遗漏最后一页的可执行权限设置并导致崩溃。部署该旧版 Linux 包时，请确认所使用的 PolyHook 二进制已包含该提交。

### English

If Linux deployments still use the older `2.5.7` package, it requires the `mem_protect` page-range calculation fix: include the starting address's page offset when rounding up the protected memory length.

- **Plugin source base**: `5892ea6a0ce7e8c48748e054e91bbf4df973cc9f`
- **PolyHook_2 dependency commit**: `298d56210b9d9e66cde8f96481d6053925c6ae15`
- **Upstream fix**: Merged in [PR #37](https://github.com/untrustedmodders/plugify-plugin-polyhook/pull/37), commit `0aadc50fb66afd5c77af4f601e5e4b25e1be7b86`

An unpatched `2.5.7` Linux binary can leave the final page of an unaligned trampoline range non-executable. Verify that legacy Linux packages include this patch before deploying them.
