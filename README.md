# hymission

`hymission` is a Hyprland plugin that provides a Mission Control-style overview with live compositor-side previews, scope-aware collection, trackpad gestures, and a workspace strip for active-workspace overview mode.

> [!IMPORTANT]
> This README focuses on installation, public usage, and user-facing configuration. The behavioral contract lives in [`docs/spec.md`](docs/spec.md).

> [!WARNING]
> Hyprland plugins run inside the compositor process. Install plugins only from sources you trust.
> `hymission` may not work correctly on NVIDIA GPUs/drivers.

> [!WARNING]
> This software is 99% vibe coded with OpenAI CodeX, but have been manual audited, warn in case you mind it.

**Inspired By Apple Mission Control**

**Referenced [hyprexpo](https://github.com/hyprwm/hyprland-plugins/tree/main/hyprexpo), [hycov](https://github.com/ernestoCruz05/hycov), and [Hyprspace](https://github.com/KZDKM/Hyprspace).**
## Features

- Mission Control-style overview with animated window previews
- Scope control with default config scope, `onlycurrentworkspace`, and `forceall`
- Mouse, keyboard, and trackpad-driven overview interaction
- Optional selected-preview expansion with local push-away animation
- Gesture-only `recommand` mode for two-sided `toggle` gestures
- Workspace strip when the current overview scope shows only the active workspace
- Multi-monitor support
- Pinned-window, special-workspace, and scrolling-layout aware behavior
- Workspace-to-workspace overview transitions without showing the native workspace animation in the middle



https://github.com/user-attachments/assets/d3e7625f-a831-474a-ac85-02dca635beda




## Installation

### Install with `hyprpm`

`hyprpm` is the preferred user-facing install path in the Hyprland ecosystem.

```sh
hyprpm update
hyprpm add https://github.com/gfhdhytghd/hymission
hyprpm enable hymission
hyprpm reload
```

If you use Hyprland's permission system, you may need to allow `hyprpm` in your config:

```lua
hl.permission("/usr/(bin|local/bin)/hyprpm", "plugin", "allow")
```

Do not also manually `hyprctl plugin load` the same plugin if you manage it through `hyprpm`.

### Manual build and reload

For local development, `hymission` uses CMake and outputs `build-cmake/libhymission.so`.

Requirements:

- Hyprland development headers for the exact Hyprland build you are running
- `cmake`
- `pkg-config`
- a C++23-capable compiler

`nlohmann/json` is bundled as a single header under `src/vendor/` (v3.12.0),
so no system package is required. Do not re-add `find_package(nlohmann_json)`.

Build:

```sh
cmake -DCMAKE_BUILD_TYPE=Release -B build-cmake
cmake --build build-cmake -j"$(nproc)"
ctest --test-dir build-cmake --output-on-failure
```

Unload (optional): only needed if a previous copy is already loaded, so you
start from a clean state. `plugin not loaded` is expected and harmless when a
path was not the active copy.

```sh
hyprctl plugin unload "$(pwd)/build-cmake/libhymission.so"
```

> If you previously built into a different directory, unload that path too,
> e.g. `build/` or `build-meson/`.

Load the freshly built copy and confirm it is active:

```sh
hyprctl plugin load "$(pwd)/build-cmake/libhymission.so"
hyprctl plugin list
```

Build outputs:

- Plugin: `build-cmake/libhymission.so`
- Layout demo: `build-cmake/hymission-layout-demo`
- Layout test: `build-cmake/hymission-mission-layout-test`
- Logic test: `build-cmake/hymission-overview-logic-test`

## Usage

### Dispatchers

```lua
hl.bind("SUPER + TAB", hl.plugin.hymission.toggle)
hl.bind("SUPER + SHIFT + TAB", function()
    hl.plugin.hymission.toggle("reverse")
end)
hl.bind("SUPER + CTRL + TAB", hl.plugin.hymission.close)
hl.bind("SUPER + C", function()
    hl.plugin.hymission.toggle("onlycurrentworkspace")
end)
hl.bind("SUPER + A", function()
    hl.plugin.hymission.toggle("forceall")
end)
hl.bind("SUPER + M", hl.plugin.hymission.debug_current_layout)
```

| Dispatcher | Description |
| --- | --- |
| `hymission:toggle` | Toggle overview. Supports `onlycurrentworkspace`, `forceall`, and the `reverse` switch-session direction modifier. |
| `hymission:open` | Open overview. Supports `onlycurrentworkspace` and `forceall`. |
| `hymission:close` | Close overview. |
| `hymission:debug_current_layout` | Compute the current layout and show a notification summary without entering overview. |

! Notice that you may only start the sapture by dispatcher, if you start hyprcapture-ui manually, it may not work correctly.
##Scope arguments:

- no argument: use the default config-driven collection scope
- `onlycurrentworkspace`: show only the current regular workspace on the anchor monitor
- `forceall`: show all regular workspaces across participating monitors and include currently visible special workspaces
- `reverse`: only for `hymission:toggle`; cycle backward in toggle switch mode. It can be combined with a scope, for example `forceall,reverse`.

### Toggle Switch Mode

`toggle_switch_mode` only affects `hymission:toggle`.

With a binding such as `hl.bind("SUPER + TAB", hl.plugin.hymission.toggle)` and:

```lua
hl.config({
    plugin = {
        hymission = {
            toggle_switch_mode = 1,
            switch_toggle_auto_next = 1,
            switch_release_key = "Super_L",
        },
    },
})
```

- the first `SUPER+TAB` opens overview as a switch session
- repeated `TAB` presses while `SUPER` stays held cycle to the next overview target
- `SUPER+SHIFT+TAB` can use `hymission:toggle,reverse` to open the switch session and select the previous target, then cycle backward on repeated presses
- releasing `SUPER` commits the current selection and exits overview

`hymission:open`, `hymission:close`, and gesture paths keep their normal behavior. Toggle switch mode is meant for modifier-backed `hymission:toggle` bindings such as `ALT+TAB` / `SUPER+TAB`.

Hymission exposes native plugin functions under `hl.plugin.hymission`:

```lua
hl.bind("SUPER + TAB", hl.plugin.hymission.toggle)
hl.bind("SUPER + A", function()
    hl.plugin.hymission.toggle("forceall")
end)
hl.bind("SUPER + S", function()
    hl.plugin.hymission.open("onlycurrentworkspace")
end)
hl.bind("SUPER + Escape", hl.plugin.hymission.close)
hl.bind("SUPER + O", function()
    hl.plugin.hymission.fullscreen({ mode = "maximized", action = "toggle" })
end)
```

Available functions:

- `hl.plugin.hymission.toggle(args?)`
- `hl.plugin.hymission.open(args?)`
- `hl.plugin.hymission.close()`
- `hl.plugin.hymission.fullscreen({ mode = "fullscreen"|"maximized", action = "toggle"|"set"|"unset" })`
- `hl.plugin.hymission.debug_current_layout()`
- `hl.plugin.hymission.dispatch(name, args?)`
- `hl.plugin.hymission.gesture(table|string, disable_inhibit?)`

`toggle` and `open` accept the optional scope arguments `forceall` and `onlycurrentworkspace`. Only `toggle` additionally accepts `reverse` as a switch-session direction modifier.

### Gestures

Register Hymission gestures through `hl.plugin.hymission.gesture(...)` instead of `hl.gesture({ action = function() ... end })` when you want continuous overview progress:

```lua
hl.plugin.hymission.gesture({
    fingers = 4,
    direction = "vertical",
    action = "toggle",
    args = "forceall",
})

hl.plugin.hymission.gesture({
    fingers = 4,
    direction = "vertical",
    action = "toggle",
    recommand = true,
})

hl.plugin.hymission.gesture({
    fingers = 4,
    direction = "vertical",
    action = "open",
    scope = "onlycurrentworkspace",
})

hl.plugin.hymission.gesture({
    fingers = 3,
    direction = "horizontal",
    action = "scroll",
    mode = "layout",
})

-- Native alternative:
-- hl.gesture({ fingers = 3, direction = "horizontal", action = "scroll_move" })

hl.plugin.hymission.gesture({
    fingers = 3,
    direction = "vertical",
    action = "workspace",
})
```

Optional gesture fields are `mods`, `scale`, and `disable_inhibit`.

Gesture notes:

- `vertical` and `horizontal` are supported for plugin-managed overview gestures; `hymission:scroll,layout` also supports `swipe`
- default gesture semantics are state-aware: hidden overview opens in the configured direction, and visible `hymission:toggle,*` overview can close in either swipe direction
- `recommand` is gesture-only and is only valid with `hymission:toggle`
- scrolling layout movement supports both `hymission:scroll,layout` and Hyprland's official `scrollMove` / Lua `scroll_move`
- workspace swipes should use `hl.plugin.hymission.gesture({ ..., action = "workspace" })`; Hymission already intercepts that path while overview is visible
- in `recommand` mode, one side opens `forceall` and the other side opens `onlycurrentworkspace`
- switching from one visible `recommand` side to the other only works in the side-changing direction; it must pass through hidden state and then cross a small transfer gap before the opposite side starts opening
- swiping the other visible `recommand` direction only exits overview back to hidden and does not continue into the opposite side
- a gesture that started from hidden can still be pulled back to cancel, but it cannot become a new visible-start close/transfer gesture until you lift and swipe again
- release still uses a `50% + velocity` commit rule

## Configuration

All user-facing settings live under `plugin.hymission` in `hl.config`.

Example:

```lua
hl.config({
    plugin = {
        hymission = {
            outer_padding_top = 92,
            outer_padding_right = 32,
            outer_padding_bottom = 32,
            outer_padding_left = 32,
            row_spacing = 32,
            column_spacing = 32,
            min_window_length = 120,
            min_preview_short_edge = 32,
            small_window_boost = 1.35,
            max_preview_scale = 0.95,
            workspace_overview_max_preview_scale = 0.95,
            min_slot_scale = 0.10,
            natural_scale_flex = 0.22,
            layout_engine = "grid",
            layout_scale_weight = 1.0,
            layout_space_weight = 0.10,

            expand_selected_window = 1,
            hover_relayout_animation = "",
            hover_relayout_duration = 140,
            hover_relayout_curve = "ease_out_cubic",
            hover_expand_scale = 1.18,
            overview_focus_follows_mouse = 1,
            multi_workspace_sort_recent_first = 1,
            niri_mode = 0,
            niri_scroll_pixels_per_delta = 1.0,
            niri_workspace_scale = 1.0,
            niri_scrolling_preview_gap = 0,
            toggle_switch_mode = 1,
            switch_toggle_auto_next = 1,
            switch_release_key = "Super_L",
            gesture_invert_vertical = 0,
            one_workspace_per_row = 0,
            only_active_workspace = 0,
            only_active_monitor = 0,
            show_special = 0,
            workspace_change_keeps_overview = 1,
            hide_hyprbars_during_overview = 0,

            workspace_strip_anchor = "left",
            workspace_strip_empty_mode = "existing",
            workspace_strip_thickness = 160,
            workspace_strip_gap = 24,
            hide_bar_when_strip = 1,
            hide_bar_animation = 1,
            hide_bar_animation_blur = 1,
            hide_bar_animation_move_multiplier = 0.8,
            hide_bar_animation_scale_divisor = 1.1,
            hide_bar_animation_alpha_end = 0,
            bar_single_mission_control = 0,
            show_focus_indicator = 0,
            pick_labels_enabled = 0,
            pick_labels_mode = "sequential",
            pick_labels_direct_activate = 0,
            backdrop_blur = 0,
            backdrop_color = "rgba(00000000)",
            focus_hover_color = "rgba(f2f7ff8c)",
            focus_selected_color = "rgba(3dc7fff2)",
            focus_hover_thickness = 2,
            focus_selected_thickness = 4,
            workspace_strip_inactive_tint_color = "rgba(00000000)",

            debug_logs = 0,
            debug_surface_logs = 0,
        },
    }
})
```

### Layout options

| Option | Type | Default | Description |
| --- | --- | --- | --- |
| `outer_padding` | int | `32` | Legacy fallback for all four edge paddings. |
| `outer_padding_top` | int | `92` | Top padding for the overview content area. |
| `outer_padding_right` | int | `32` | Right padding for the overview content area. |
| `outer_padding_bottom` | int | `32` | Bottom padding for the overview content area. |
| `outer_padding_left` | int | `32` | Left padding for the overview content area. |
| `row_spacing` | int | `32` | Vertical spacing between preview rows. |
| `column_spacing` | int | `32` | Horizontal spacing between preview columns. |
| `min_window_length` | int | `120` | Minimum edge length used before layout scoring. |
| `min_preview_short_edge` | int | `32` | Minimum rendered short edge for previews, used to keep ultra-wide, ultra-tall, or very small windows recognizable. |
| `small_window_boost` | float | `1.35` | Weight boost applied to smaller windows during layout. |
| `max_preview_scale` | float | `0.95` | Maximum preview scale for all-workspace / multi-workspace overview. |
| `workspace_overview_max_preview_scale` | float | `0.95` | Maximum preview scale for active-workspace overview, including niri direct overview. |
| `min_slot_scale` | float | `0.10` | Minimum allowed slot scale. |
| `natural_scale_flex` | float | `0.22` | Natural-engine-only free scale range. Values are clamped to `0.0` - `0.25`; recent-first multi-workspace ordering keeps earlier windows visibly larger, while natural layouts may use larger per-window scale differences to fill sparse space. |
| `layout_engine` | string | `grid` | Geometry solver. `grid` keeps the existing row-search layout; `natural`, `apple`, `expose`, and `mission-control` enable the Apple-like natural solver that tries to preserve original window positions while removing overlap. The natural engine attempts every window count and only uses row-search as an emergency fallback if solving fails. |
| `layout_scale_weight` | float | `1.0` | Weight of preview scale in the layout scoring pass. |
| `layout_space_weight` | float | `0.10` | Weight of space utilization in the layout scoring pass. |
| `one_workspace_per_row` | bool | `0` | Keep each workspace on its own row instead of searching for the best row count. |

### Behavior options

| Option | Type | Default | Description |
| --- | --- | --- | --- |
| `expand_selected_window` | bool | `1` | Enlarge the selected preview and push nearby previews away without reshuffling the whole overview grid. Uses the overview-selected target, which usually follows hover when `overview_focus_follows_mouse = 1`. |
| `hover_relayout_animation` | string | empty | Hyprland animation leaf used for selected-preview hover relayout, for example `windowsMove`. When set to a valid leaf, Hyprland's animation tree controls speed and supports both bezier and spring curves. Invalid or empty values fall back to `hover_relayout_duration` / `hover_relayout_curve`. |
| `hover_relayout_duration` | float | `140` | Fallback selected-preview hover relayout duration in milliseconds. Values are clamped to `0` - `2000`; `0` completes immediately. Ignored when `hover_relayout_animation` resolves to a valid Hyprland animation leaf. |
| `hover_relayout_curve` | string | `ease_out_cubic` | Fallback selected-preview hover relayout easing curve. First tries a Hyprland registered bezier name such as `default`, `linear`, or `easeOutQuint`; otherwise supports `ease_in_cubic`, `ease_out_cubic`, and `ease_in_out_cubic`, with invalid values falling back to `ease_out_cubic`. Ignored when `hover_relayout_animation` is active. |
| `hover_expand_scale` | float | `1.18` | Preferred selected-preview scale multiplier used by `expand_selected_window`. Values are clamped to `1.0` - `2.0`, and layout bounds may cap the visible result. |
| `overview_focus_follows_mouse` | bool | `1` | Keep the overview selection aligned with hover, and sync real focus when allowed. Hover retargeting is frame-coalesced for smoother animation, and multi-workspace overview stays visually anchored when real focus crosses workspaces. |
| `multi_workspace_sort_recent_first` | bool | `1` | Multi-workspace overview only. When enabled, `forceall` and any default overview scope that spans multiple workspaces place more recently used windows earlier in the grid, filling left-to-right then top-to-bottom. |
| `niri_mode` | bool | `0` | Enable niri-like overflow behavior for the edge workspace strip. This is opt-in and does not turn the strip into the main overview content. |
| `niri_scroll_pixels_per_delta` | float | `1.0` | Multiplier for `hymission:scroll,layout` movement outside overview. A value of `1.0` maps roughly one `gestures:workspace_swipe_distance` of finger travel to one viewport of scrolling-layout movement. Native `scrollMove` ignores this option. |
| `niri_workspace_scale` | float | `1.0` | Niri mode strip thumbnail scale inside the configured strip thickness. Values are clamped to `0.05` - `1.0`; `1.0` uses the full strip cross-axis size. |
| `niri_scrolling_preview_gap` | int | `0` | Extra gap in pixels between niri direct scrolling-layout preview cells along the scrolling axis. In horizontal scrolling layouts this is the horizontal preview gap. |
| `toggle_switch_mode` | bool | `1` | Turn `hymission:toggle` into a toggle-only switch session. Intended for modifier-backed bindings such as `ALT+TAB` / `SUPER+TAB`. |
| `switch_toggle_auto_next` | bool | `1` | Toggle switch mode only. When enabled, the first switch-mode `toggle` both opens overview and advances to the next target. |
| `switch_release_key` | string | `Super_L` | Toggle switch mode only. Release of this key commits the current selection and closes the switch session. Supports keysym names such as `Alt_L` / `Super_L` and `code:N`, and release tracking is resilient to missing per-window release events. |
| `gesture_invert_vertical` | bool | `0` | Invert the plugin-managed vertical overview gesture direction. |
| `only_active_workspace` | bool | `0` | Restrict the default scope to the active regular workspace per participating monitor. |
| `only_active_monitor` | bool | `0` | Restrict the default scope to the monitor under the cursor. |
| `show_special` | bool | `0` | Include currently visible special workspaces in the default scope. |
| `workspace_change_keeps_overview` | bool | `1` | Keep overview open when switching workspaces in active-workspace scope. |
| `hide_hyprbars_during_overview` | bool | `0` | Suppress drawing of official `hyprbars` title bars while overview renders, without changing their reserved decoration space. This is a no-op unless `hyprbars` is loaded. |
| `show_focus_indicator` | bool | `0` | Render selected and hovered preview focus chrome. |
| `pick_labels_enabled` | bool | `0` | Show keyboard pick labels on previews and enable direct keyboard selection in the configured `pick_labels_mode`. Reuses `close_button_color` / `close_button_glyph_color` / `close_button_size` for styling; previews too small for a legible chip skip drawing it but remain selectable. |
| `pick_labels_mode` | string | `sequential` | `sequential` keeps the numbered `1`-`9`, `A1`-`Z9` scheme. `spatial` maps the physical ANSI alphanumeric block to preview centers across the participating monitors. Up to 36 windows receive distinct single-key labels; denser layouts share a primary key and show a two-key route such as `FF` or `FR`. |
| `pick_labels_direct_activate` | bool | `0` | Only applies when `pick_labels_enabled = 1`. `0` only moves the selection (still requires `Return` to confirm, same as arrow keys); `1` activates and closes overview immediately when a pick label is hit. |
| `show_window_icons` | bool | `1` | Draw the app icon centered above each overview preview. |
| `show_window_titles` | bool | `1` | Reveal the app name above a preview while it is hovered or selected. |

Behavior notes:

- In multi-workspace overview, hover-driven real focus may still cross workspaces, but the overview grid stays anchored instead of rebuilding on every workspace change.
- In active-workspace overview, workspace changes still use the dedicated overview-to-overview transition path.
- Toggle switch mode keeps current hover semantics: if `overview_focus_follows_mouse = 1`, moving the pointer can still retarget the final committed selection during the switch session.
- In `sequential` mode, past the 9th window a letter key (`A`-`Z`) arms a ~1.5s prefix waiting for its digit (e.g. `A` then `2` picks `A2`); any other key cancels the prefix without losing its own normal effect (e.g. `Esc` still closes overview).
- In `spatial` mode, key positions and activation use physical keycodes, while badge text follows the active keyboard's current XKB layout automatically (using its unshifted level, so Shift/Caps Lock do not change the badge). A shared primary waits up to ~1.5s for the same key (center) or an adjacent key in the labelled direction; equivalent adjacent keys in that direction are also accepted.

### Appearance options

Color options use Hyprland color syntax such as `rgba(rrggbbaa)`.

| Option | Type | Default | Description |
| --- | --- | --- | --- |
| `backdrop_color` | color | `rgba(00000000)` | Optional full-monitor overview backdrop tint. Keep transparent for blur without dimming. |
| `backdrop_blur` | bool | `0` | Blur the full-monitor overview backdrop. |
| `focus_hover_color` | color | `rgba(f2f7ff8c)` | Hover focus outline color. |
| `focus_selected_color` | color | `rgba(3dc7fff2)` | Selected focus outline color. |
| `focus_title_color` | color | `rgba(ffffffff)` | Selected window title text color. |
| `focus_hover_thickness` | float | `2` | Hover focus outline thickness. |
| `focus_selected_thickness` | float | `4` | Selected focus outline thickness. |
| `close_button_color` | color | `rgba(29292eeb)` | Close button idle fill color. |
| `close_button_hover_color` | color | `rgba(f24d47f2)` | Close button hover fill color. |
| `close_button_glyph_color` | color | `rgba(fffffffa)` | Close button glyph color. |

### Workspace strip options

| Option | Type | Default | Description |
| --- | --- | --- | --- |
| `workspace_strip_anchor` | string | `left` | Strip anchor. Supports `top`, `left`, and `right`. |
| `workspace_strip_empty_mode` | string | `existing` | Empty-workspace strip policy. `existing` only shows real workspaces; `continuous` inserts the next missing numbered workspace in each positive-id gap without expanding named-workspace spans. |
| `workspace_strip_thickness` | int | `160` | Strip thickness. |
| `workspace_strip_gap` | int | `24` | Gap between the strip and the main overview content. |
| `workspace_strip_background_color` | color | `rgba(0812243d)` | Strip band background color. |
| `workspace_strip_inactive_color` | color | `rgba(0d17262e)` | Inactive workspace card fill. |
| `workspace_strip_active_color` | color | `rgba(1a2e523d)` | Active workspace card fill. |
| `workspace_strip_empty_color` | color | `rgba(0f1a292e)` | Synthetic empty workspace card fill. |
| `workspace_strip_new_color` | color | `rgba(1c293b42)` | New-workspace card fill. |
| `workspace_strip_hover_tint_color` | color | `rgba(ffffff0f)` | Tint drawn over the hovered workspace thumbnail. |
| `workspace_strip_active_tint_color` | color | `rgba(5794f21a)` | Tint drawn over the active workspace thumbnail. |
| `workspace_strip_inactive_tint_color` | color | `rgba(00000000)` | Tint drawn over inactive workspace thumbnails. Defaults to transparent. |
| `workspace_strip_plus_color` | color | `rgba(f7fbffe0)` | Plus glyph color for the new-workspace card. |
| `hide_bar_when_strip` | bool | `1` | Replace matching exclusive bars with a short self-blur / slide / scale proxy handoff while the strip is shown. |
| `hide_bar_animation` | bool | `1` | Enable the bar handoff animation. When disabled, matching bars hide/show instantly with the strip. |
| `hide_bar_animation_blur` | bool | `1` | Enable blur during the bar handoff. When disabled, the handoff keeps alpha / move / scale only. |
| `hide_bar_animation_move_multiplier` | float | `0.8` | Multiplier for how much the bar follows strip movement. Clamped to `0.0` - `2.0`. `1.0` matches full strip travel and `2.0` doubles it. |
| `hide_bar_animation_scale_divisor` | float | `1.1` | Bar scale divisor at full strip reveal. A value of `n` means the proxy scales to `1 / n` of its original size at maximum. `1.0` disables scaling. |
| `hide_bar_animation_alpha_end` | float | `0.0` | Final bar proxy alpha when the strip is fully revealed. Clamped to `0.0` - `1.0`. `0.0` fully fades out; higher values keep part of the bar visible. |
| `bar_single_mission_control` | bool | `0` | Multi-workspace overview only. Keep this at `0` to preserve the bar's normal numbered workspace display. When enabled, the bar workspace list collapses to a single `Mission Control` entry and the other regular overview workspaces are renamed to an internal hidden prefix so bars can filter them out. Intended for Waybar `ignore-workspaces`. |

The workspace strip is shown when the current overview scope displays only the active workspace.
By default it only shows real workspaces plus the trailing new-workspace card. In `continuous` mode, synthetic empty workspaces progressively expose numbered gaps one slot at a time and render the monitor background/wallpaper when available; the trailing new-workspace card keeps its dedicated `+` styling.
With `niri_mode = 1`, the strip stays in the configured edge band and the main overview remains the scaled window overview. The strip uses monitor-aspect workspace thumbnails, centers the active workspace on open, and allows the thumbnail list to overflow instead of shrinking every workspace into view. Tiled `scrolling` layout previews use `workspace_overview_max_preview_scale` on the non-scrolling axis and may overflow along the scrolling axis, so gesture panning moves the centered row/column instead of shrinking the whole tape into view. Both `hymission:scroll,layout` and Lua `scroll_move` can scroll the `scrolling` layout inside the niri overview; workspace switching continues to use `hl.plugin.hymission.gesture({ ..., action = "workspace" })`.

### Optional Waybar Single-Entry Setup

Leave `bar_single_mission_control = 0` if you want `hyprland/workspaces` to keep showing the usual numbered workspaces.

If you explicitly want `hyprland/workspaces` to collapse to a single `Mission Control` button while multi-workspace overview is visible:

1. Set `bar_single_mission_control = 1` in `hl.config({ plugin = { hymission = { ... } } })`.
2. Add an `ignore-workspaces` rule that hides the plugin's temporary names:

```jsonc
"hyprland/workspaces": {
  "all-outputs": true,
  "disable-scroll": true,
  "on-click": "activate",
  "persistent_workspaces": {},
  "ignore-workspaces": ["^__hymission_hidden__:"]
}
```

This keeps normal workspace names untouched outside overview. While overview is open, the anchor workspace remains `Mission Control` and the other regular overview workspaces are renamed to the hidden prefix so Waybar drops them from the module.

### Debug options

| Option | Type | Default | Description |
| --- | --- | --- | --- |
| `debug_logs` | bool | `0` | Enable overview debug logging. |
| `debug_surface_logs` | bool | `0` | Enable more verbose surface-level debug logging. |

## Development

Useful commands:

```sh
./build-cmake/hymission-layout-demo
./build-cmake/hymission-layout-demo --list-scenes
./build-cmake/hymission-layout-demo --scene forceall --engine natural --output /tmp/hymission-forceall-natural.svg
./build-cmake/hymission-layout-demo --scene forceall --engine grid --output /tmp/hymission-forceall-grid.svg
./build-cmake/hymission-layout-demo --stress 5000 --seed 1 --output /tmp/hymission-stress-worst.svg
./build-cmake/hymission-mission-layout-test
./build-cmake/hymission-overview-logic-test
hyprctl dispatch hymission:debug_current_layout
```

`hymission-layout-demo` runs the geometry solver without loading the Hyprland plugin. In SVG output, dashed rectangles are source window geometry and solid rectangles are overview targets. Built-in scenes include `forceall`, `default`, `stacked`, `right-biased`, and `workspace-rows`. It also reports gravity, heatmap balance, motion, and x/y inversion metrics; SVG output draws heat cells, the screen center, and the target-area centroid. `--stress` generates random pathological scenes and writes the worst-scoring case for solver tuning.

Project docs:

- [`docs/spec.md`](docs/spec.md): behavior and user-facing semantics
- [`docs/architecture.md`](docs/architecture.md): controller, hooks, and state-machine structure
- [`docs/research.md`](docs/research.md): layout tradeoffs and prior-art notes
- [`docs/workspace_strip_plan.md`](docs/workspace_strip_plan.md): strip-specific implementation planning
- [`docs/todo.md`](docs/todo.md): current gaps and next steps
- [`devlog/`](devlog): implementation notes for recent iterations

## Notes

- The repository includes a root [`hyprpm.toml`](hyprpm.toml) manifest, which is expected by `hyprpm`.
- For inclusion in the official `hyprland-plugins` repository, Hyprland asks plugin authors to coordinate with the repository maintainer first.
