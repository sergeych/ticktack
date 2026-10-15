#!/usr/bin/env python3
# SPDX-License-Identifier: CC-BY-4.0
# Copyright (c) 2026 sergeych and Ticktack contributors
"""Extract the final recorded clock strike; retain the overlapping natural decay."""
from pathlib import Path
import array
import subprocess
import wave

ROOT = Path(__file__).resolve().parent
SOUNDS = ROOT / 'sounds'
source = SOUNDS / '05-diarchangeli-striking-12-preview.mp3'
CACHE = ROOT / '.cache' / 'audio'
CACHE.mkdir(parents=True, exist_ok=True)
decoded = CACHE / '05-diarchangeli-striking-12-decoded.wav'
if not decoded.exists():
    subprocess.run(['gst-launch-1.0', '-q', 'filesrc', f'location={source}', '!',
                    'decodebin', '!', 'audioconvert', '!', 'audioresample', '!',
                    'audio/x-raw,format=S16LE,rate=44100,channels=2', '!',
                    'wavenc', '!', 'filesink', f'location={decoded}'], check=True)
with wave.open(str(decoded)) as w:
    rate, channels = w.getframerate(), w.getnchannels()
    assert (rate, channels, w.getsampwidth()) == (44100, 2, 2)
    recording = array.array('h', w.readframes(w.getnframes()))

# Final substantial amplitude rise at about 32.728s. Keep 6ms before it.
# No spectral filtering/denoising: preceding strokes' residual resonance remains.
start, end = round(32.722 * rate), round(58.0 * rate)
strike = recording[start * channels:end * channels]
frames = len(strike) // channels
for frame in range(frames):
    gain = min(1.0, frame / (.003 * rate), (frames - 1 - frame) / (.4 * rate))
    for channel in range(channels):
        i = frame * channels + channel
        strike[i] = round(strike[i] * max(0.0, gain))

def write(name, samples):
    with wave.open(str(CACHE / name), 'wb') as w:
        w.setparams((channels, 2, rate, 0, 'NONE', 'not compressed'))
        w.writeframes(samples.tobytes())
    print(name, f'{len(samples)/channels/rate:.3f}s',
          f'peak {max(abs(v) for v in samples)/32768:.3f}')

write('09-diarchangeli-single-strike.wav', strike)
print('Source cut: 32.722–58.000 seconds; 3ms fade-in, 400ms fade-out; no pitch change.')
