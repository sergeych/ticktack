#!/usr/bin/env python3
# SPDX-License-Identifier: CC-BY-4.0
# Copyright (c) 2026 sergeych and Ticktack contributors
"""Prepare mono PCM assets for mixing in the native audio clock."""
from pathlib import Path
import array
import subprocess
import sys
import wave
ROOT = Path(__file__).resolve().parent
SOUNDS = ROOT / 'sounds'
OUT = SOUNDS / 'chimes'
OUT.mkdir(exist_ok=True)
CACHE = ROOT / '.cache' / 'audio'
# Recreate intermediate strike/decoded audio rather than shipping redundant WAVs.
subprocess.run([sys.executable, str(ROOT / 'prepare_strike.py')], check=True)
for minute, name in [(15, '06-diarchangeli-quarter-15-preview.mp3'),
                     (30, '07-diarchangeli-quarter-30-preview.mp3'),
                     (45, '08-diarchangeli-quarter-45-preview.mp3')]:
    destination = OUT / f'quarter-{minute}.wav'
    temporary = destination.with_suffix('.tmp.wav')
    subprocess.run(['gst-launch-1.0', '-q', 'filesrc', f'location={SOUNDS/name}', '!',
                    'decodebin', '!', 'audioconvert', '!', 'audioresample', '!',
                    'audio/x-raw,format=S16LE,rate=44100,channels=1', '!',
                    'wavenc', '!', 'filesink', f'location={temporary}'], check=True)
    temporary.replace(destination)

def mono(name):
    with wave.open(str(CACHE/name)) as w:
        assert (w.getframerate(), w.getsampwidth(), w.getnchannels()) == (44100, 2, 2)
        data = array.array('h', w.readframes(w.getnframes()))
    return array.array('h', (round((data[i]+data[i+1])/2) for i in range(0,len(data),2)))

def write(name, data):
    destination = OUT / name
    temporary = destination.with_suffix('.tmp.wav')
    with wave.open(str(temporary), 'wb') as w:
        w.setparams((1,2,44100,0,'NONE','not compressed'))
        w.writeframes(data.tobytes())
    temporary.replace(destination)

# Retain the complete hourly phrase, stopping just before the first hour strike.
# Its boundary is masked by the new strike attack; taper 4ms to avoid a splice click.
prefix = mono('05-diarchangeli-striking-12-decoded.wav')[:round(15.285*44100)]
for i in range(round(.004*44100)):
    prefix[-1-i] = round(prefix[-1-i]*i/round(.004*44100))
write('hour-prefix.wav', prefix)
write('strike.wav', mono('09-diarchangeli-single-strike.wav'))
for name in sorted(OUT.glob('*.wav')):
    with wave.open(str(name)) as w:
        print(name.name, f'{w.getnframes()/w.getframerate():.3f}s', name.stat().st_size)
