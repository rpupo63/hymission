# AGENTS.md

## What this repo is

A personal Hyprland plugin that mimics Apple's Mission Control. It is an in-process
`.so` loaded into the compositor — **there is no daemon, no helper process, no systemd
unit, and no socket**. If you find yourself looking for one, there isn't one to find.

Fork of `wilf`'s hymission (`upstream` remote); `origin` is `rpupo63/hymission`.

## How it is actually loaded on this machine

**Not by hyprpm.** As of 2026-08-13 `hyprpm list` is empty and `~/.local/share/hyprpm/`
does not exist. The load path is a single idempotent `exec` in
`~/.config/hypr/autostart.conf`, which runs at startup *and* on every config reload:

1. `hyprctl plugin load /home/beto/Projects/hymission/build-cmake/libhymission.so`
   — only if the plugin isn't already loaded.
2. `hyprctl keyword source ~/.config/hypr/hymission-setup.conf`
   — only if a reload wiped the dynamic keywords.

The runtime `plugin { hymission { … } }` block and the `SUPER+grave` bind live in
**`~/.config/hypr/hymission-setup.conf`**. They cannot go in a statically-sourced file:
Hyprland parses those before the plugin exists and rejects the unknown keywords.

`~/.config/hypr/hyprland.conf` grants exactly one `permission = … plugin, allow`, for the
`build-cmake/` path above. Adding a grant for a path with no `.so` behind it is how you end
up authorizing an abandoned binary — two such grants were removed on 2026-08-13.

> Previous versions of this file claimed hyprpm managed this repo as a local source, and
> that the active config lived in `~/.config/HyprV/hypr/hyprland-plugins.conf`. Both were
> false here — that path does not exist on this machine. Corrected 2026-08-13.

## Building

CMake only. `meson.build` was removed 2026-08-13 (never built here). `hyprpm.toml` exists
but is unused; it shells out to the same CMake build and writes to the same
`build-cmake/libhymission.so`.

Builds against the **stock** `hyprland` package headers in `/usr/include/hyprland` —
`hyprland-git` is not required and is not installed.

**Nothing rebuilds the plugin when Hyprland updates.** That is what broke it on the
0.55 → 0.56 bump. Rebuild by hand after every Hyprland upgrade, or migrate to hyprpm.

## Reload safety

- Treat `hyprpm update` as a **live plugin reload, not just a build step. It can
  unload/load the enabled plugin and may crash Hyprland if the plugin is active or
  currently rendering overview.** Do not run it automatically while the session is in use.
- `hyprpm update` builds without loading. Use `hyprpm reload -f` to swap the live plugin.
- Do not mix a hyprpm-managed instance with manual `hyprctl plugin load` / `unload` in the
  same live session — duplicate instances fight over hooks and destabilize Hyprland.
- Manual `hyprctl plugin load` / `unload` both require an absolute path.

## Known hazard: global options the plugin mutates behind your back

The plugin force-sets global Hyprland options via `setConfigKeyword`
(`src/overview_controller.cpp:450-464`), which falls back to synthesizing Lua and `eval`ing
it when `hyprctl keyword` refuses. Measured live against the 0.5.0 build on 2026-08-13:

| Option | Scope of the override | Restored by | Site |
|---|---|---|---|
| `input:follow_mouse` → `0` | overview open, **plus a deliberate tail past close** | next pointer motion or click | `:6737-6762` |
| `scrolling:follow_focus` → `0` | only around scroll-driven mouse moves | `handleMouseMove` | `:6765-6796` |
| `animations:enabled` → `0` | **transient only** — a few frames | self-arming `restoreDelay` timer | `:6799-6858` |

None of these is a leak. Two earlier versions of this section said otherwise; both were
wrong, in opposite directions. What is actually true, measured against the 0.5.0 build:

- **`animations:enabled` is never observably 0.** `setAnimationsEnabledOverride` takes a
  `restoreDelay` and arms a self-restoring `CEventLoopTimer`, so suppression lasts a few
  frames during a transition. Practical consequence: a native
  `animation = workspaces, …, slide` in `looknfeel.conf` does **not** double-animate
  against the plugin's own transition — the plugin suppresses it for exactly that window.

- **`input:follow_mouse` stays `0` after the overview closes, on purpose.** At `:11238`
  the close path branches:

  ```cpp
  if (!shouldPreserveExitFocus) {
      setInputFollowMouseOverride(false);              // restore immediately
      m_restoreInputFollowMouseAfterPostClose = false;
  } else {
      m_restoreInputFollowMouseAfterPostClose = true;  // defer
  }
  ```

  When the overview closes *onto a window you selected*, restoring follow-mouse right away
  would let whatever the cursor happens to be sitting over immediately steal that focus
  back. So the override is held and discharged on the next real pointer event — `:2848`
  (`handleMouseMove`, after `m_ignorePostCloseMouseMoveCount` counts down) or `:2896`
  (`handleMouseButton`). Verified 2026-08-13: toggle open → `0`, toggle closed → still
  `0`, jiggle the mouse → back to `1`.

  **This is why black-box testing it lies.** Driving the overview with
  `hyprctl dispatch hymission:toggle` and then reading `hyprctl getoption` generates no
  pointer motion, so the deferred restore never fires and the value reads `0` forever.
  That artifact is what the previous version of this section recorded as a leak. Any test
  of these options must generate real input events before sampling.

The one path that genuinely strands an override is `setConfigKeyword` *failing* during
restore: the `m_*Overridden` flag is only cleared after success, so a failure leaves the
option forced with a red `[hymission] failed to restore …` notification on screen.
**Recovery:** `hyprctl reload`.

Two asymmetries worth knowing if you ever touch this code:

- `setAnimationsEnabledOverride` guards against recording the forced value as the backup
  (`if (m_animationsEnabledBackup == 0) return;`). The other two have no equivalent — they
  are currently protected only by the `if (m_*Overridden) return;` re-entry guard. Copying
  the animations guard into them is cheap hardening, not a bug fix; nothing reachable today
  gets past the re-entry guard.
- `setScrollingFollowFocusOverride`'s workspace gate (`:6768-6769`) sits *above both*
  branches, so it can refuse the restore as well as the disable. Only reachable if a
  scrolling workspace exists at disable time and not at restore time — i.e. it is tied to
  the niri support that is a deletion candidate anyway.

## Config coupling worth knowing before you change anything

- **`only_active_workspace = 1` is load-bearing well beyond the overview.** Setting it to
  `0` makes the plugin silently swallow every `workspace` dispatch while the overview is
  open, which kills all 20+ `SUPER+[1-9]` / `SUPER ALT+[1-9]` / `name:0` binds in
  `bindings.conf`. It looks cosmetic. It is not.
- The plugin **wraps the user's native horizontal workspace swipe** through three
  independent mechanisms: parse-time keyword interception (`:5141`), retroactive
  `replaceNativeWorkspaceGestures` (`:2368-2370`, re-runs on every config reload), and raw
  function hooks (`:6984-6986`). That gesture therefore runs through plugin code even with
  the overview closed — every gate requires `isVisible()`, so closed-overview behavior
  falls through to native.
- In `~/.config/hypr/input.conf`, `scale:` **must** precede the `workspace` action token.
  With `scale:` after it, the plugin's gesture parser falls through to a plain native swipe
  and the 0/negative-workspace handoff never runs.
- The 0/negative-workspace swipe direction (`:6217-6230`) is inferred empirically from
  `input:natural_scroll` + `gestures:workspace_swipe_invert`. Change either and the
  direction can silently flip.
