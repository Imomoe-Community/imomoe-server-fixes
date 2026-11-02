## Summary / 变更概述

<!-- Briefly describe what this PR changes and why / 简要说明本次 PR 修改了什么以及原因 -->

## Affected Modules / 涉及模块

- [ ] `chat_admission` (Chat admission / 聊天准入)
- [ ] `hostage_compat` (Hostage rescue & T pickup / 人质救援与背负兼容)
- [ ] `spawn_capacity` (Spawn point capacity & manual tools / 出生点容量与手动工具)
- [ ] `spectator_compat` (Spectator capacity / 观察者容量同步)
- [ ] Build / CI / Documentation (构建、CI 或文档)

## Verification / 测试验证

- [ ] `cmake --preset policy && cmake --build --preset policy && ctest --preset policy` passed
- [ ] `python -m unittest discover -s tests -p "test_*.py"` passed
- [ ] Paired SDK plugin build & callback tests passed (if runtime / native / gamedata code changed)
