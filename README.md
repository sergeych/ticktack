# Ticktack

A small Linux desktop pendulum clock: one mechanical stroke per second, natural
quarter-hour phrases and hourly strikes. A tiny native C process feeds the
installed PipeWire player; a GNOME 50 extension provides the controls.

Original project contributions are licensed under **CC BY 4.0**. See
[LICENSE.md](LICENSE.md), [sound credits and licenses](SOUNDS-LICENSES.md) and
[readable credits](credits.html). Keep these notices and `licenses/` with
redistributed source, binaries and audio assets. No NonCommercial recordings
are included.

## Install

```sh
sh install.sh
systemctl --user daemon-reload
gnome-extensions enable ticktack@sergeych.local
```

Requires a C compiler with libc headers to build, and `pw-cat`, a systemd user
session and `flock` to run. No Chromium, JVM or Python process runs in the
background. Python and GStreamer are only needed to recreate audio assets.

The installer creates `~/.local/bin/ticktack`, a GNOME application-launcher entry,
a user systemd service and the GNOME extension. Playback starts on request and remembers its enabled state across logout and
reboot. Login restores playback only if it was last enabled. GNOME may require a new login to discover a newly
installed local extension. Avoid restarting a working Wayland session for this.
The installer also copies license/credit documents to `~/.local/share/ticktack/`
(or `$XDG_DATA_HOME/ticktack/`).

## Controls

```sh
ticktack start
ticktack stop
ticktack toggle
ticktack status
ticktack volume 25                 # ticking, 0..100, live and persistent
ticktack volume                    # show saved ticking volume
ticktack chime off                 # keep ticking, disable scheduled chimes
ticktack chime on
ticktack chime-volume 25
ticktack chime-test 15              # audition the first quarter now
ticktack chime-test 30
ticktack chime-test 45
ticktack chime-test hour 3
ticktack chime-test hour 12
ticktack credits                   # open credits and license links
```

The application-launcher entry toggles playback. The panel extension has a master
switch, independent ticking/chime volume sliders, a quarter-hour chime switch,
a Listen now submenu and Credits and licenses. Disabling the extension leaves
playback available through the CLI and launcher.

The master switch, CLI start/stop, launcher and auditions save playback intent.
Logout stops the audio service without clearing that preference.

Settings are plain files under `~/.config/ticktack/` (or
`$XDG_CONFIG_HOME/ticktack/`). Defaults: ticking 35%, chime volume 35%, scheduled
chimes enabled. Volume is independent of system master volume.

Open [listen.html](listen.html) to audition the prepared sounds. Incoming
recordings and all processing are documented in SOUNDS-LICENSES.md.

## Playback and scheduling

While running, local time schedules:

- **:15 / :30 / :45:** the complete corresponding recorded phrase and decay.
- **:00:** the hourly phrase followed by 1–12 strikes; midnight/noon = 12.

Ticking continues beneath the chime in one persistent audio stream. Successive
strikes overlap at approximately 1.585-second intervals. An audition replaces
any currently sounding chime, starts playback if necessary and works even with
scheduled chimes disabled. Previous auditions never replay on startup.

The scheduler uses an absolute `CLOCK_REALTIME` timerfd for the next quarter,
cancels/rearms on system-clock adjustments and handles DST using future local
minute boundaries. Events more than two seconds late are discarded: no catch-up
chimes after suspend or when starting mid-quarter. The timer is disarmed when
scheduled chimes are off. Normal audio/device buffering still applies; this is
not a sample-accurate wall-clock synchronizer.

The C process memory-maps the WAV assets, caches the scaled ticking loop and
mixes chimes only while active. A bounded 4096-byte pipe feeds one persistent
`pw-cat`. Volume changes arrive through atomic setting-file replacement and
inotify. The main loop blocks on pipe readiness, configuration events and the
quarter timer; there is no periodic clock/status polling and no process per
stroke or chime. systemd `Type=notify` ensures the settings watcher is ready
before an audition command is sent. The GNOME extension uses mixer/file events;
its only timers coalesce slider interaction.

## Recreate the sounds

Only the five selected incoming MP3 previews are retained, together with the
six WAVs used at runtime. Unused candidate recordings, intermediate WAVs and
redundant listening demos have been removed.

```sh
python3 prepare_sound.py
python3 prepare_chimes.py
```

`prepare_sound.py` selects twelve different ticking strokes and aligns them on
a one-second PCM grid, preserving pitch. `prepare_chimes.py` invokes
`prepare_strike.py`, prepares the three complete quarter-hour phrases, extracts
the hourly prefix and converts the final strike to mono. The final strike keeps
its natural decay and some preceding residual resonance. Decoded/intermediate
files go into `.cache/audio/`, excluded from version control. License and change
notices for all source and runtime files are in SOUNDS-LICENSES.md.

## Validation

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror tests/check.c -o /tmp/ticktack-check
/tmp/ticktack-check
```

Checks cover quarter boundaries, midnight, forward/backward DST, late-event
suppression, hour counts, overlapping tails and mix headroom. At default gains,
twelve strikes plus ticking peak at 7961/32767. A captured-PCM integration check
also verified an audition over uninterrupted ticking, complete decay and return
to ticking. The extension was enabled/disabled/enabled in an isolated GNOME 50
session without reported extension errors; the live service passed its readiness
handshake and played the first-quarter audition.

## Uninstall

Stop playback and disable the extension. Remove `~/.local/bin/ticktack`,
`~/.config/systemd/user/ticktack.service`, `~/.config/autostart/ticktack.desktop`,
`~/.local/share/applications/ticktack.desktop`,
`~/.local/share/gnome-shell/extensions/ticktack@sergeych.local` and
`~/.local/share/ticktack`; run `systemctl --user daemon-reload`.
The project folder and saved volume settings can be retained.
