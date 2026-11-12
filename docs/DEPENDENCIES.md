# Dependency Inputs / 依赖与构建输入说明

## 配对 SDK 构建输入 / Paired SDK Build

编译完整的 `imomoe_server_fixes` 插件时，请使用下表所列的固定版本与同一工作树下的配对构建产物（[`imomoe_server_fixes.pplugin`](../imomoe_server_fixes.pplugin) 中声明的最低运行版本仅作为加载兼容下限，不能替代编译期的配对 SDK 要求）：

When building the full `imomoe_server_fixes` plugin, use the pinned versions below from a single configured SDK worktree (the minimum version constraints in [`imomoe_server_fixes.pplugin`](../imomoe_server_fixes.pplugin) are runtime compatibility floors, not a substitute for these paired build inputs):

| 组件 / Input | 验证版本或来源 / Validated Version or Source |
| --- | --- |
| **S2SDK** | `3.4.2`, [signed fork commit `e358fcb`](https://github.com/Mai-Ooizumi/plugify-plugin-s2sdk/tree/e358fcba28f05956f6276278b4bba7c575805c86) |
| **SourceSDK** | S2SDK 子模块指针 [`492d328`](https://github.com/Wend4r/sourcesdk/tree/492d3281ecad150131da50ae69c63edca51d261b) |
| **Plugify Host** | `3.1.0` |
| **Plugify C++ Language Module** | `2.4.5` |
| **PolyHook** | 最低运行版本 `>= 2.5.9`；Windows／Linux 配对验证使用官方 [`2.5.9`](https://github.com/untrustedmodders/plugify-plugin-polyhook/releases/tag/v2.5.9) / runtime minimum `>= 2.5.9`; validated with official `2.5.9` on Windows and Linux |
| **plugify-gen** | `1.2.15`（仅在需要从 `bindings/s2sdk.pplugin` 重新生成绑定时使用 / for intentional binding regeneration） |

请检出上述固定提交的 S2SDK 并递归初始化其子模块（`git submodule update --init --recursive`）。SourceSDK 公共头文件、生成的 Protobuf 头文件（`external/sourcesdk/protos`）、静态库 `tier0_perproject` 和 `math` 以及运行时 `tier0` 导入库均来自该 S2SDK 工作树及其已完成的配对构建。具体 CMake 参数请参考 [README 构建指南](../README.md#build-the-plugin-with-the-paired-sdk)。

Check out S2SDK at the fixed commit above with recursive submodules initialized. The SourceSDK headers, generated protobuf headers (`external/sourcesdk/protos`), static `tier0_perproject` and `math` archives, and runtime `tier0` import library must all come from that same configured S2SDK build tree. See the [README build instructions](../README.md#build-the-plugin-with-the-paired-sdk) for the exact CMake arguments.
