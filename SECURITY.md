# Security Policy / 安全策略

## 报告安全问题 / Reporting a Vulnerability

如果你在本插件中发现了可能导致 CS2 服务端崩溃、内存越界读写、权限绕过（如客户端可执行仅限服务端控制台的命令）或其他安全隐患的问题，请**不要**直接在公开 Issue 中披露利用细节。

If you discover a vulnerability that could cause a CS2 dedicated server crash, out-of-bounds memory access, privilege escalation (e.g., client execution of server-console-only commands), or other security issues, please **do not** disclose exploit details in a public GitHub issue.

请通过以下方式私下联系维护者并提供复现信息：
Please report the issue privately to the maintainers with reproduction details:

- 通过 GitHub 私信或仓库维护者主页联系方式联系 **Mai Ooizumi**。
  Contact **Mai Ooizumi** via GitHub or the contact channels listed on the maintainer's profile.
- 请附上：CS2 服务端构建号（Build ID / Timestamp）、操作系统（Windows / Steam Runtime Linux）、`sf_status` 输出以及最小复现步骤。
  Please include: CS2 server build number (Build ID / Timestamp), OS (Windows / Steam Runtime Linux), `sf_status` output, and minimal reproduction steps.

## 支持范围 / Supported Versions

由于本插件包含针对特定 CS2 服务端二进制（`server.dll` / `libserver.so`）特征码与偏移的校验，我们仅对当前主分支（`main`）所适配的最新服务端版本提供安全修复支持。当 CS2 更新导致二进制校验不匹配时，相关底层补丁会自动拒绝加载（Fail-closed）以保护服务端安全。

Because this plugin validates binary signatures and image metadata against specific CS2 server builds (`server.dll` / `libserver.so`), security fixes are provided for the latest release on the `main` branch. On unrecognized game updates, binary-pinned hooks and patches fail closed automatically rather than writing to unverified memory.
