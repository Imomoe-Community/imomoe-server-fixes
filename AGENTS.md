# Imomoe-ServerFixes-CS2

## Scope and runtime invariants

This is a standalone C++ Plugify plugin. Keep gameplay in FunRounds; this
repository owns chat admission, hostage compatibility, spawn capacity and
spectator capacity. Read the affected behavior and configuration in README.md
before changing it.

Engine work runs on the game thread:
- Keep shared map-generation and schema-readiness tracking in a plugin-level
  lifecycle component rather than inside an individual capability module;
  disabled modules release their feature hooks when no cleanup is pending.
- Validate map/round/life identity and full entity handles at callback entry and
  before delayed or re-entrant actions, then reuse the validated state within the
  same synchronous call chain. Read in-process engine structures directly after
  ABI/signature verification instead of probing self-memory with OS syscalls
  (`process_vm_readv` / `ReadProcessMemory`).
- Separate hot-path (`OnPluginUpdate`, `Touch`) state updates from `sf_status`
  diagnostic string formatting. Cache stable ConVar handles, schema offsets and
  module binary identity with explicit reset points; read live ConVar values,
  entity validity and external-override lease state at the decision boundary.
- Restore only values still owned by this plugin. Track registration ownership
  per command, event and shared detour so partial registration, rollback or
  unload failures remain visible and retryable through the standard cleanup path.

## Dependencies and generated inputs

For SDK, ABI, gamedata or build-input changes, read docs/DEPENDENCIES.md and the
paired-build instructions in README.md. Keep generated-s2sdk/ checked in under
`generated-s2sdk/external/plugify/include` and regenerate it only from
bindings/s2sdk.pplugin. Preserve the generated imports, license notices and
build-specific native guards together.

## Verification and review

Use the policy preset and Python tests (`tests/` and `tools/tests/`) documented
in README.md for SDK-independent or build-script changes. Runtime callback or
native changes also require the matching production build and affected callback
tests. New native boundaries pass minimal-plugin checks on Windows and Linux
before gameplay relies on them; ABI, hook lifecycle and recovery changes require
independent review. Filter out speculative guards without a reachable failure
path, reuse valid evidence for unchanged inputs and keep human acceptance
platform-specific.
