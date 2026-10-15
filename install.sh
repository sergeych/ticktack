#!/bin/sh
# SPDX-License-Identifier: CC-BY-4.0
# Copyright (c) 2026 sergeych and Ticktack contributors
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
config=${XDG_CONFIG_HOME:-$HOME/.config}
data=${XDG_DATA_HOME:-$HOME/.local/share}
mkdir -p "$base/bin" "$HOME/.local/bin" "$config/systemd/user" "$config/ticktack" "$data/applications"
cc -std=c11 -Os -Wall -Wextra -Werror -Wl,-z,relro,-z,now -o "$base/bin/.ticktack-audio.new" "$base/ticktack.c"
strip "$base/bin/.ticktack-audio.new"
mv "$base/bin/.ticktack-audio.new" "$base/bin/ticktack-audio"
chmod +x "$base/ticktack"
ln -sfn "$base/ticktack" "$HOME/.local/bin/ticktack"
if [ ! -e "$config/ticktack/volume" ]; then "$base/bin/ticktack-audio" --volume 35; fi
if [ ! -e "$config/ticktack/chime-volume" ]; then "$base/bin/ticktack-audio" --chime-volume 35; fi
if [ ! -e "$config/ticktack/chime-enabled" ]; then "$base/bin/ticktack-audio" --chime-enabled 1; fi
cat > "$config/systemd/user/ticktack.service" <<EOF
[Unit]
Description=Ticktack pendulum clock

[Service]
Type=notify
NotifyAccess=main
ExecStart="$base/bin/ticktack-audio" --play "$base/sounds/04-nightcustard-aligned-loop-12s.wav"
KillMode=control-group
TimeoutStopSec=3
NoNewPrivileges=yes
EOF
cat > "$data/applications/ticktack.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Ticktack
Comment=Toggle the pendulum clock
Exec="$HOME/.local/bin/ticktack" toggle
Icon=alarm-symbolic
Terminal=false
Categories=Utility;
Keywords=clock;tick;pendulum;
Actions=Stop;

[Desktop Action Stop]
Name=Stop ticking
Exec="$HOME/.local/bin/ticktack" stop
EOF
extension="$data/gnome-shell/extensions/ticktack@sergeych.local"
mkdir -p "$extension"
cp "$base/extension/extension.js" "$base/extension/metadata.json" "$extension/"
docs="$data/ticktack"
mkdir -p "$docs/licenses"
cp "$base/LICENSE.md" "$base/SOUNDS-LICENSES.md" "$base/credits.html" "$docs/"
cp "$base/licenses/CC-BY-4.0.txt" "$base/licenses/CC0-1.0.txt" "$docs/licenses/"
desktop-file-validate "$data/applications/ticktack.desktop"
printf 'Installed audio engine, CLI, desktop entry and GNOME extension.\n'
printf 'Run: systemctl --user daemon-reload\n'
printf 'Then enable: gnome-extensions enable ticktack@sergeych.local\n'
