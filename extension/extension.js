// SPDX-License-Identifier: CC-BY-4.0
// Copyright (c) 2026 sergeych and Ticktack contributors
import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import St from 'gi://St';

import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as PanelMenu from 'resource:///org/gnome/shell/ui/panelMenu.js';
import * as PopupMenu from 'resource:///org/gnome/shell/ui/popupMenu.js';
import * as Slider from 'resource:///org/gnome/shell/ui/slider.js';
import {getMixerControl} from 'resource:///org/gnome/shell/ui/status/volume.js';

export default class TicktackExtension extends Extension {
    enable() {
        this._alive = true;
        this._updating = false;
        this._volumeTimers = new Map();
        this._volumeControls = new Map();
        this._command = GLib.build_filenamev([GLib.get_home_dir(), '.local', 'bin', 'ticktack']);
        this._configDir = Gio.File.new_for_path(GLib.build_filenamev([GLib.get_user_config_dir(), 'ticktack']));
        this._cancellable = new Gio.Cancellable();
        this._button = new PanelMenu.Button(0.0, 'Ticktack');
        this._icon = new St.Icon({icon_name: 'alarm-symbolic', style_class: 'system-status-icon'});
        this._button.add_child(this._icon);
        this._switch = new PopupMenu.PopupSwitchMenuItem('Ticktack', false);
        this._button.menu.addMenuItem(this._switch);
        this._switch.connect('toggled', (_item, state) => {
            if (this._updating) return;
            this._switch.reactive = false;
            this._switch.label.text = state ? 'Starting…' : 'Stopping…';
            this._run([state ? 'start' : 'stop'], () => {
                this._switch.reactive = true;
                this._switch.label.text = 'Ticktack';
                this._syncPlaying();
            });
        });
        this._addVolume('volume', 'Ticking');
        this._button.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this._chimeSwitch = new PopupMenu.PopupSwitchMenuItem('Quarter-hour chime', true);
        this._button.menu.addMenuItem(this._chimeSwitch);
        this._chimeSwitch.connect('toggled', (_item, enabled) => {
            if (!this._updating) this._run(['chime', enabled ? 'on' : 'off'], () => this._syncSettings());
        });
        this._addVolume('chime-volume', 'Chimes');
        const preview = new PopupMenu.PopupSubMenuMenuItem('Listen now');
        for (const minute of [15, 30, 45])
            preview.menu.addAction(`Quarter :${minute}`, () => this._run(['chime-test', `${minute}`]));
        preview.menu.addAction('Hour · 3 strikes', () => this._run(['chime-test', 'hour', '3']));
        preview.menu.addAction('Hour · 12 strikes', () => this._run(['chime-test', 'hour', '12']));
        this._button.menu.addMenuItem(preview);
        this._button.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this._button.menu.addAction('Credits and licenses', () => this._run(['credits']));
        this._monitor = this._configDir.monitor_directory(Gio.FileMonitorFlags.NONE, null);
        this._monitor.connect('changed', (_monitor, file) => {
            if (['volume', 'chime-volume', 'chime-enabled'].includes(file.get_basename())) this._syncSettings();
        });
        this._mixer = getMixerControl();
        this._mixerSignals = ['stream-added', 'stream-removed', 'state-changed'].map(signal =>
            this._mixer.connect(signal, () => this._syncPlaying()));
        this._button.menu.connect('open-state-changed', (_menu, open) => {
            if (open) { this._syncPlaying(); this._syncSettings(); }
        });
        Main.panel.addToStatusArea(this.uuid, this._button);
        this._syncSettings();
        this._syncPlaying();
    }

    _addVolume(setting, title) {
        const row = new PopupMenu.PopupBaseMenuItem({activate: false});
        row.add_child(new St.Label({text: title, y_align: Clutter.ActorAlign.CENTER}));
        const slider = new Slider.Slider(0.35);
        slider.style = 'min-width: 130px;';
        slider.accessible_name = `Ticktack ${title.toLowerCase()} volume`;
        row.add_child(slider);
        const percent = new St.Label({text: '35%', y_align: Clutter.ActorAlign.CENTER});
        row.add_child(percent);
        this._button.menu.addMenuItem(row);
        this._volumeControls.set(setting, {slider, percent});
        slider.connect('notify::value', () => {
            percent.text = `${Math.round(slider.value * 100)}%`;
            if (this._updating) return;
            if (this._volumeTimers.has(setting)) GLib.source_remove(this._volumeTimers.get(setting));
            this._volumeTimers.set(setting, GLib.timeout_add(GLib.PRIORITY_DEFAULT, 80, () => {
                this._volumeTimers.delete(setting);
                this._run([setting, `${Math.round(slider.value * 100)}`]);
                return GLib.SOURCE_REMOVE;
            }));
        });
    }

    _run(args, done = null) {
        try {
            const process = Gio.Subprocess.new([this._command, ...args], Gio.SubprocessFlags.STDERR_PIPE);
            process.communicate_utf8_async(null, this._cancellable, (proc, result) => {
                try {
                    const [, , error] = proc.communicate_utf8_finish(result);
                    if (!this._alive) return;
                    if (!proc.get_successful()) Main.notifyError('Ticktack', error.trim() || 'Command failed');
                } catch (error) {
                    if (this._alive) Main.notifyError('Ticktack', error.message);
                }
                if (this._alive) done?.();
            });
        } catch (error) {
            Main.notifyError('Ticktack', error.message);
            done?.();
        }
    }

    _readNumber(setting, fallback) {
        const file = this._configDir.get_child(setting);
        if (!file.query_exists(null)) return fallback;
        const [, bytes] = file.load_contents(null);
        return Number(new TextDecoder().decode(bytes).trim());
    }

    _syncSettings() {
        this._updating = true;
        try {
            for (const [setting, {slider, percent}] of this._volumeControls) {
                if (this._volumeTimers.has(setting)) continue;
                const volume = this._readNumber(setting, 35);
                if (!Number.isInteger(volume) || volume < 0 || volume > 100) continue;
                slider.value = volume / 100;
                percent.text = `${volume}%`;
            }
            this._chimeSwitch.setToggleState(this._readNumber('chime-enabled', 1) === 1);
        } catch (error) {
            console.warn(`Ticktack settings: ${error.message}`);
        } finally {
            this._updating = false;
        }
    }

    _syncPlaying() {
        const playing = this._mixer.get_streams().some(stream =>
            stream.get_name() === 'Ticktack' || stream.get_description() === 'Ticktack');
        this._updating = true;
        this._switch.setToggleState(playing);
        this._icon.opacity = playing ? 255 : 120;
        this._button.accessible_name = playing ? 'Ticktack — playing' : 'Ticktack — stopped';
        this._updating = false;
    }

    disable() {
        this._alive = false;
        for (const timer of this._volumeTimers?.values() ?? []) GLib.source_remove(timer);
        this._volumeTimers?.clear();
        this._cancellable?.cancel();
        this._monitor?.cancel();
        for (const id of this._mixerSignals ?? []) this._mixer.disconnect(id);
        this._mixerSignals = [];
        this._button?.destroy();
        this._button = null;
        this._volumeControls?.clear();
    }
}
