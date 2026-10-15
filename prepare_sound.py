#!/usr/bin/env python3
# SPDX-License-Identifier: CC-BY-4.0
# Copyright (c) 2026 sergeych and Ticktack contributors
"""Place recorded tick/tock attacks on a one-second PCM grid (stdlib + GStreamer)."""
from pathlib import Path
import array
import math
import statistics
import subprocess
import wave

ROOT = Path(__file__).resolve().parent
SOUNDS = ROOT / 'sounds'
SOURCE = SOUNDS / '02-nightcustard-grandfather-preview.mp3'
CACHE = ROOT / '.cache' / 'audio'
CACHE.mkdir(parents=True, exist_ok=True)
DECODED = CACHE / '02-nightcustard-decoded.wav'
if not DECODED.exists():
    subprocess.run(['gst-launch-1.0', '-q', 'filesrc', f'location={SOURCE}', '!',
                    'decodebin', '!', 'audioconvert', '!',
                    'audio/x-raw,format=S16LE,channels=1,rate=44100', '!',
                    'wavenc', '!', 'filesink', f'location={DECODED}'], check=True)
with wave.open(str(DECODED)) as source:
    rate = source.getframerate()
    assert source.getnchannels() == 1 and source.getsampwidth() == 2
    samples = array.array('h', source.readframes(source.getnframes()))

# Detect first substantial attack, rather than the loudest peak later in the mechanism.
bin_size = 220
energy = [math.sqrt(sum(x*x for x in samples[i:i+bin_size])/bin_size)
          for i in range(0, len(samples)-bin_size, bin_size)]
onsets = []
for second in range(2, 58):
    lo, hi = [int(t*rate/bin_size) for t in (second-.3, second+.3)]
    region = energy[lo:hi]
    baseline = statistics.median(sorted(region)[:len(region)//2])
    height = max(region)-baseline
    index = next(i for i in range(lo, hi-2)
                 if energy[i] > baseline+height*.22
                 and max(energy[i:i+3]) > baseline+height*.4)
    onsets.append(index*bin_size)
intervals = [(b-a)/rate for a,b in zip(onsets, onsets[1:])]
print('Estimated alternating median intervals:',
      *(f'{statistics.median(intervals[i::2]):.4f}s' for i in range(2)))

# Twelve different strokes keep the recorded mechanical variation; timing alone is reset.
# An equal 10ms pre-roll precedes every detected attack. No pitch/time stretching.
strokes = []
for onset in onsets[:12]:
    start = onset - round(.010*rate)
    stroke = array.array('h', samples[start:start+round(.65*rate)])
    for i in range(len(stroke)):
        fade = min(1., i/(.003*rate), (len(stroke)-1-i)/(.12*rate))
        stroke[i] = round(stroke[i]*max(0., fade))
    strokes.append(stroke)
gain = .55*32767/max(abs(x) for stroke in strokes for x in stroke)

def render(name, seconds):
    output = array.array('h', [0])*(seconds*rate)
    for second in range(seconds):
        stroke = strokes[second % len(strokes)]
        for i, value in enumerate(stroke):
            output[second*rate+i] = round(value*gain)
    destination = SOUNDS / name
    temporary = destination.with_suffix('.tmp.wav')
    with wave.open(str(temporary), 'wb') as target:
        target.setparams((1, 2, rate, 0, 'NONE', 'not compressed'))
        target.writeframes(output.tobytes())
    temporary.replace(destination)
    print(name, seconds, 'seconds; stroke grid =', rate, 'samples')

render('04-nightcustard-aligned-loop-12s.wav', 12)
