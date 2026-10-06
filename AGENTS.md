# AGENTS.md

## What this repo is

A personal Hyprland plugin that mimics Apple's Mission Control. It is an in-process
`.so` loaded into the compositor — **there is no daemon, no helper process, no systemd
unit, and no socket**. If you find yourself looking for one, there isn't one to find.

Fork of `wilf`'s hymission (`upstream` remote); `origin` is `rpupo63/hymission`.

## Live Hyprland and Omarchy config is part of every bug and every edit

This plugin runs **inside Hyprland** on Omarchy. It is not a sidecar. Plugin code
intercepts gestures, can swallow `workspace` dispatches, and force-sets live
compositor options via `setConfigKeyword`. The user's compositor and desktop
config live **outside this repo**, in `~/.config/hypr/` and `~/.config/omarchy/`.
Theme reloads, binds, gestures, and `omarchy-*` hooks can look like plugin bugs.

Read those live files **before** editing plugin source. When the user reports a
bug, treat Hyprland and Omarchy config as first-class suspects — not an
afterthought after a plugin-only patch.

```xml
<rule id="hyprland-config-coupling">
  <scope>Any hymission bug report, bugfix, behavior change, reload, or live-session test</scope>
  <must>Before editing plugin source, read the live Hyprland and Omarchy config that can affect the same surface: ~/.config/hypr/ and ~/.config/omarchy/.</must>
  <must>When the user reports a bug, consider those live configs first. Binds, gestures, workspace dispatches, theme reload, follow_mouse, animations, and omarchy hooks can conflict with or overwrite plugin behavior.</must>
  <must>Check hymission-setup.conf (plugin block, SUPER+grave bind), autostart.conf (hymission-load; also runs on omarchy theme reload), hyprland.conf (plugin permission), bindings.conf (workspace/overview binds), input.conf (gestures, scale: token order, natural_scroll), looknfeel.conf (workspace animations), and ~/.config/omarchy/hooks/ when the change or bug can interact with them.</must>
  <must>Confirm the plugin is not permanently overwriting a user option (setConfigKeyword / input:follow_mouse / animations:enabled / scrolling:follow_focus) unless that override is documented and restored.</must>
  <must>Confirm a new plugin behavior does not conflict with an existing bind, gesture, or workspace dispatch already defined in ~/.config/hypr/.</must>
  <must>If a live Hyprland or Omarchy config file must change, use skill omarchy and edit ~/.config/hypr/ or ~/.config/omarchy/ — never ~/.local/share/omarchy/.</must>
  <must_not>Start a hymission edit or bugfix from plugin source alone while skipping the live Hyprland and Omarchy config.</must_not>
  <must_not>Treat a successful CMake build, or a plugin-source-only diff, as proof the live session will behave correctly.</must_not>
  <must_not>Add plugin permissions, binds, or plugin { hymission { } } blocks to a statically sourced Hyprland file that parses before the plugin is loaded.</must_not>
  <ref>Known hazard: global options the plugin mutates; Config coupling worth knowing before you change anything; skill omarchy</ref>
</rule>
```

## How it is actually loaded on this machine

**Not by hyprpm.** On this machine `hyprpm list` is empty and `~/.local/share/hyprpm/`
does not exist. The load path is `exec = ~/.config/hypr/scripts/hymission-load` in
`~/.config/hypr/autostart.conf`, which runs at startup *and* on every config reload:

1. `hyprctl plugin load /home/beto/Projects/quality-of-life-laptop-improvements/hymission/build-cmake/libhymission.so`
   — only if the plugin isn't already loaded.
2. `hyprctl keyword source ~/.config/hypr/hymission-setup.conf`
   — only if a reload wiped the dynamic keywords.

**Both steps are taken under `flock`, and that is not decoration.** Hyprland runs `exec`
on the initial config parse *and* again on omarchy's post-login theme reload, so at cold
boot two copies overlap. When the guards were inline `||` checks, both copies saw zero
binds and both sourced the file: two `SUPER+grave` binds, and two copies of
`gesture = 3, vertical, dispatcher, hymission:toggle`, which makes a 3-finger vertical
swipe toggle the overview twice and look dead. A warm `hyprctl reload` never reproduces
it — the two calls are far enough apart that check-then-act works. Diagnosed on a
cold boot, after the warm-session test said the config was fine.

`unbind` would not have been enough: Hyprland has no unbind for `gesture`, and
`hyprctl gestures` returns `unknown request`, so the doubled gesture is neither
removable nor observable. Serializing is what fixes both halves. The loader logs to
`journalctl --user -t hymission-load` and self-checks the resulting bind count.

The runtime `plugin { hymission { … } }` block and the `SUPER+grave` bind live in
**`~/.config/hypr/hymission-setup.conf`**. They cannot go in a statically-sourced file:
Hyprland parses those before the plugin exists and rejects the unknown keywords.

`~/.config/hypr/hyprland.conf` grants exactly one `permission = … plugin, allow`, for the
`build-cmake/` path above. Adding a grant for a path with no `.so` behind it is how you end
up authorizing an abandoned binary — two such grants were removed after that incident.

> Previous versions of this file claimed hyprpm managed this repo as a local source, and
> that the active config lived in `~/.config/HyprV/hypr/hyprland-plugins.conf`. Both were
> false here — that path does not exist on this machine. Corrected after that incident.

## Building

CMake only. `meson.build` was removed (never built here). `hyprpm.toml` exists
but is unused; it shells out to the same CMake build and writes to the same
`build-cmake/libhymission.so`.

Builds against the **stock** `hyprland` package headers in `/usr/include/hyprland` —
`hyprland-git` is not required and is not installed.

**Nothing rebuilds the plugin when Hyprland updates.** That is what broke it on the
0.55 → 0.56 bump. Rebuild by hand after every Hyprland upgrade, or migrate to hyprpm.

## Notifications are mirrored to the desktop daemon

Hyprland's notification overlay is render-only: it takes no input, keeps no history, and
writes nothing to `hyprland.log` or socket2. Verified on 0.56.2 —
`src/notification/NotificationOverlay.cpp` registers one listener (`monitor.focused`) for
damage and nothing else, so a click never reaches a notification and a timed-out one is
unrecoverable. Notifications are therefore also sent to the desktop daemon (swaync here)
by `src/notify_mirror.cpp`, which has two halves.

**Half one — the plugin's own notifications.** Two chokepoints feed it. New notifications
must go through one of them, not through `HyprlandAPI::addNotification` directly:

- `OverviewController::notify` — controller notifications. Urgency is inferred from the
  color: `r > 0.9 && g < 0.3` means failure, which maps to `-u critical` so swaync holds
  it until dismissed.
- `notifyFailure` in `main.cpp` — every plugin-init failure. Always critical.

**Half two — Hyprland's own notifications**, which never pass through this plugin and
never reach D-Bus either. `CCompositor::performUserChecks()` and the monitor code call
`Notification::overlay()->addNotification` directly, so the banners that matter most —
the `.conf`-removed-in-0.57 deadline, a rejected monitor scale, a missing watchdog, failed
assets — were simply lost when their 15s timer ran out. `startOverlayMirror()` polls
`Notification::overlay()->getNotifications()` every 500ms and mirrors anything new under
app name `Hyprland`. It is started last in `PLUGIN_INIT` and torn down in `PLUGIN_EXIT`.

Three things about that half are load-bearing:

- **Urgency comes from the icon, not only the color.** `ICON_ERROR` or a red banner maps to
  `-u critical`; everything else is `normal` and still persists in swaync's control center.
  Hyprland's plain warnings use `CHyprColor{}` (all zeros), so a colour-only rule would
  have read them as non-failures.
- **Seen notifications are tracked by `WP<CNotification>`, not by text.** Weak, so tracking
  never keeps a banner alive past its own timer; by identity, so a genuine repeat of the
  same message still mirrors.
- **`mirrorNotification` claims its own overlay entry** by finding the matching text and
  marking it seen, so half one and half two never double-send. It deliberately claims *one*
  matching entry rather than snapshotting the overlay — snapshotting would swallow an
  unmirrored Hyprland banner that happened to be on screen at the same moment. Verified
  live: one `hymission:debug_current_layout` produces exactly one D-Bus `Notify`.

**Upgrade hazard:** the mirror spawns via `Config::Supplementary::executor()->spawnRaw()`
(`config/supplementary/executor/Executor.hpp`), an *internal* Hyprland API, not
`PluginAPI.hpp`. It can move or disappear on any Hyprland bump, so check it alongside the
rebuild above. `spawnRaw` ends in `execl("/bin/sh", "-c", args)`, so anything interpolated
into the command must stay shell-quoted (`shellQuote` in `notify_mirror.cpp`).

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
it when `hyprctl keyword` refuses. Measured live against the 0.5.0 build:

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
  (`handleMouseButton`). Verified: toggle open → `0`, toggle closed → still
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

See **Live Hyprland and Omarchy config is part of every bug and every edit** —
these are the known load-bearing couplings, not an exhaustive list. Re-read the
live `~/.config/hypr/` and `~/.config/omarchy/` files for the surface you are
changing or debugging.

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
