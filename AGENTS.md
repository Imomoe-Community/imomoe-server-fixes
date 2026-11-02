# Imomoe-ServerFixes-CS2

## Scope and ownership

This is a standalone C++ Plugify plugin. Keep gameplay in FunRounds; this
repository owns chat admission, hostage compatibility, spawn capacity and
spectator capacity. Read the affected behavior and configuration in README.md
before changing it.

Engine work runs on the game thread. Validate map/round/life identity and full
entity handles before delayed actions. Restore only values still owned by this
plugin. Shared detours retain other consumers; unregister callbacks before
unloading and keep failed cleanup visible and retryable.

## Dependencies and generated inputs

For SDK, ABI, gamedata or build-input changes, read docs/DEPENDENCIES.md and the
paired-build instructions in README.md. Keep generated-s2sdk/ checked in and
regenerate it only from bindings/s2sdk.pplugin. Preserve the generated imports,
license notices and build-specific native guards together.

## Verification

Use the policy preset and Python migration tests documented in README.md for
SDK-independent changes. Runtime callback or native changes also require the
matching production build and affected callback tests. New native boundaries
pass minimal-plugin checks on Windows and Linux before gameplay relies on them;
ABI, hook lifecycle and recovery changes require independent review. Reuse
valid evidence for unchanged inputs and keep human acceptance platform-specific.
