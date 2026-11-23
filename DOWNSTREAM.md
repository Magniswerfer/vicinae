# Downstream fork

This fork carries a few personal macOS additions on top of [vicinaehq/vicinae](https://github.com/vicinaehq/vicinae).

## Branches

- `main` mirrors `upstream/main` and gets no fork changes.
- `downstream` holds the additions as a few commits on top of upstream.

Remotes: `origin` is this fork and `upstream` is vicinaehq.

## Features

- **Window layouts** (Settings → Extensions → Window Management): halves, quarters, thirds, maximize variants,
  center, resize, move, next/previous display and restore. Repeating a half cycles ½ → ⅔ → ⅓. Preferences cover the
  window gap, size cycling, and the Almost Maximize and Reasonable Size percentages.
- **Hyper key** (Settings → Extensions → Hyper Key): Caps Lock acts as ⌘⌃⌥⇧ (Shift optional) and can be used in any
  shortcut, where it shows as ✦. A quick press can send Escape, and Shift+Caps Lock toggles the real Caps Lock.
- **Clipboard quick paste** (Settings → Extensions → Clipboard → Clipboard History): with *Quick paste with the
  hotkey* on, the history opens while the hotkey's modifiers are held, the key again or the arrows pick an item,
  and letting go pastes it (the plain clipboard if nothing was picked, so a tap acts like ⌘V). Escape cancels.
  Vicinae's other hotkeys are paused while it is open. *Paste as plain text* drops formatting from every paste
  out of the history, with the other choice in the action panel.

## Updating from upstream

```bash
git fetch upstream && git rebase upstream/main
./scripts/install-downstream.sh
git push --force-with-lease
```

`install-downstream.sh` builds a release with LTO, signs it, replaces `/Applications/Vicinae.app` and relaunches it.

## Things to know

- **Signing.** Builds are signed with the same Apple Development certificate each time
  (`VICINAE_CODESIGN_IDENTITY` overrides it), so macOS keeps the granted permissions across rebuilds. Ad-hoc
  signing would make macOS ask again after every build. Expect to re-grant once when the certificate is renewed.
- **Updates.** The version is stamped `<upstream tag>-downstream`, which turns off the built-in updater, so an
  official release can't overwrite this build.
- **Permissions.** Window layouts need Accessibility. The hyper key needs Accessibility and Input Monitoring:
  without Input Monitoring the key tap is created but never receives a keystroke. Vicinae asks for both, and keeps
  Caps Lock as a normal Caps Lock until they are granted. Input Monitoring takes effect after relaunching Vicinae.
- **Caps Lock after a crash.** The Caps Lock → F18 remap is undone on quit and cleared at the next start. If
  Vicinae is killed, Caps Lock does nothing until Vicinae starts again or the Mac restarts.
- **Official build.** A backup of the official 0.30.1 app is at `~/Vicinae-0.30.1-official-backup.zip`.
- **Tests.** The window layout maths has Catch2 tests in `src/server/tests/window-layout.cpp`. The project's test
  target only builds on Linux; on macOS compile it directly against Qt Core and Catch2.
