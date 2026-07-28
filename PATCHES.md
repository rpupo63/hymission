# Local Patches

Custom changes on top of upstream hymission. Re-apply with:
```bash
git am patches/*.patch
```

Or cherry-pick by commit hash after rebasing onto a new upstream tag.

## 0001 — App icons + right-click menu build support
**File:** `patches/0001-feat-add-app-icons-and-right-click-menu-build-suppor.patch`  
**Commit:** `32c9e54`  
**Problem:** Upstream has no app icon rendering or right-click window context menus.  
**Solution:** Adds `src/app_icon.cpp` / `src/app_icon.hpp` with icon-fetching logic; wires GTK/GdkPixbuf into CMake.  
**Functions touched:** `app_icon.cpp` (new), `CMakeLists.txt`, `src/overview_controller.hpp`

## 0002 — Fix preview coordinate offsets
**File:** `patches/0002-Fix-preview-coordinate-offsets.patch`  
**Commit:** `060cb04`  
**Problem:** Window previews rendered at wrong positions on multi-monitor setups.  
**Solution:** Corrects offset math in the thumbnail render path.  
**Functions touched:** `overview_controller.cpp` (thumbnail layout)

## 0003 — Step into 0/negative workspaces at native-swipe edge
**File:** `patches/0003-feat-step-into-0-negative-workspaces-at-the-native-s.patch`  
**Commit:** `72f0895`  
**Problem:** Swiping past workspace 1 did nothing; couldn't reach workspace 0 or negative indices.  
**Solution:** Detects edge-of-workspace-list and hands the swipe off to a new workspace.  
**Functions touched:** `overview_controller.cpp`, `overview_controller.hpp` (new edge-swipe helpers)

## 0004 — Dispatch negative-workspace handoff via keybind manager
**File:** `patches/0004-fix-dispatch-negative-workspace-handoff-via-keybind-.patch`  
**Commit:** `2d34455`  
**Problem:** Edge-swipe handoff in 0003 used a wrong dispatch path; direction was inferred incorrectly.  
**Solution:** Routes through `CKeybindManager` and infers direction from swipe sign.  
**Functions touched:** `overview_controller.cpp` (edge-direction inference)

## 0005 — Keep overview live during workspace swipes
**File:** `patches/0005-Keep-overview-live-during-workspace-swipes-instead-o.patch`  
**Commit:** `430b542`  
**Problem:** Swiping between workspaces while in overview dismissed the overlay, flashing the desktop.  
**Solution:** Tracks an active workspace transition (`m_workspaceTransition`); keeps overview rendered until committed. Adds session-handoff docs.  
**Functions touched:** `overview_controller.cpp` (`beginOverviewWorkspaceSwipeTransition*`, `clearOverviewWorkspaceTransition`, `commitOverviewWorkspaceTransition`)

## 0006 — Show next-workspace apps during live swipe
**File:** `patches/0006-Show-next-workspace-overview-apps-during-live-swipe.patch`  
**Commit:** `ad41aaf`  
**Problem:** During a workspace swipe in overview, only the current workspace's windows were visible; the target workspace appeared empty.  
**Solution:** Renders the target workspace's thumbnails in real time as the swipe progresses.  
**Functions touched:** `overview_controller.cpp` (swipe render loop, `updateOverviewWorkspaceSwipeGesture`)

## 0007 — Add overview app icons and right-click window menu
**File:** `patches/0007-Add-overview-app-icons-and-right-click-window-menu.patch`  
**Commit:** `b20fd33`  
**Problem:** Overview showed bare window thumbnails; no way to close/move windows from overview.  
**Solution:** Renders per-window app icons from desktop files; adds right-click context menu (close, move to workspace).  
**Functions touched:** `overview_controller.cpp` (icon overlay, context menu event handler)

## 0008 — Port features to Hyprland 0.56.0 API
**File:** `patches/0008-fix-port-cherry-picked-features-to-Hyprland-0.56.0-A.patch`  
**Commit:** `cac87de`  
**Problem:** API changes in Hyprland 0.56.0 broke several of the above features (method renames, signal changes).  
**Solution:** Updates call sites to 0.56.0 signatures; adjusts includes.  
**Functions touched:** `overview_controller.cpp`, `overview_controller.hpp` (API compatibility layer)

## 0009 — Guard null deref + commit-path debug logging
**File:** `patches/0009-fix-guard-null-deref-and-add-commit-path-debug-loggi.patch`  
**Commit:** `ac49861`  
**Problem:** Rare null `pMonitor` deref in `preBlurQueued` when clearing a transition during an overview render cycle; hard to diagnose commit-path issues without logging.  
**Solution:** Adds null guard; adds gated debug logging to `commitOverviewWorkspaceTransition` and related callbacks.  
**Functions touched:** `overview_controller.cpp` (`preBlurQueued`, `commitOverviewWorkspaceTransition`, `handleWindowSetChange`, `handleWorkspaceChange`)

## 0010 — Fix right-swipe direction lock in overview
**File:** `patches/0010-fix-skip-begin-event-delta-to-fix-right-swipe-direct.patch`  
**Commit:** `7f8d112`  
**Problem:** Swiping right (higher workspace) in overview always failed. `CHymissionWorkspaceTrackpadGesture::begin` called `updateOverviewWorkspaceSwipeGesture(distance(e))` on the begin event. For `dir=NONE`, `distance()` returns `delta.size()` (always positive ≈42). With `workspace_swipe_invert=1` this became -42, immediately locking `lockedStep=-1` before the real direction was known.  
**Solution:** Remove the `distance(e)` call from `begin`; let the first update event (which carries a classified, signed direction) set the direction lock.  
**Functions touched:** `overview_controller.cpp` (`CHymissionWorkspaceTrackpadGesture::begin`, `workspaceSwipeBeginHook`)

## 0011 — Pass targetWorkspace to clearOverviewWorkspaceTransition
**File:** `patches/0011-fix-pass-targetWorkspace-to-clearOverviewWorkspaceTr.patch`  
**Commit:** `95b911e`  
**Problem:** After a swipe committed, `commitOverviewWorkspaceTransition` called `clearOverviewWorkspaceTransition()` without a target — causing the wrong workspace to show after the animation (screens appeared stacked).  
**Solution:** Pass `targetWorkspace` to `clearOverviewWorkspaceTransition`. Also adds strip-management helpers (`activateStripTargetByStep`, `stripIndexForOwnerWorkspace`, `refreshWorkspaceStripActivity`, `syncFocusDuringOverviewToOwnerWorkspace`) and gated debug logging to transition-clear sites.  
**Functions touched:** `overview_controller.cpp` (`commitOverviewWorkspaceTransition`, `clearOverviewWorkspaceTransition`, new strip helpers)
