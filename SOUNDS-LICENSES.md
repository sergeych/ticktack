# Sound credits, licenses and modifications

Please retain this file with distributed audio assets and application packages.
All recordings are supplied as-is, without warranties. No author endorses Ticktack.

## Ticking — nightcustard

**Ticking Grandfather Clock**, by **nightcustard**, published 18 February 2022.

- Original: https://freesound.org/people/nightcustard/sounds/620573/
- License: **Creative Commons Attribution 4.0 International (CC BY 4.0)**
- License link: https://creativecommons.org/licenses/by/4.0/
- Full terms and disclaimer: [licenses/CC-BY-4.0.txt](licenses/CC-BY-4.0.txt)

Files:

| File | Changes by Ticktack |
| --- | --- |
| `sounds/02-nightcustard-grandfather-preview.mp3` | First approximately 60 seconds of the public MP3 preview, cut on complete MP3 frames. This is an excerpt, not the complete original. |
| `sounds/04-nightcustard-aligned-loop-12s.wav` | Decode to mono PCM16 at 44.1 kHz; select twelve distinct strokes; align their detected attacks at one-second intervals; apply short fades, shorten tails and adjust overall gain. Original pitch retained. |

The source and adapted ticking are distributed under CC BY 4.0.

Suggested attribution:

> Ticking: “Ticking Grandfather Clock” by nightcustard, CC BY 4.0,
> https://freesound.org/people/nightcustard/sounds/620573/ .
> Excerpted and edited for Ticktack: mono conversion, stroke selection,
> timing alignment, fades and gain adjustment.

## Quarter-hour and hourly chimes — DiArchangeli

Recordings by **DiArchangeli**, published 6 February 2019, from the
**Grandfather Clock Sounds** pack:
https://freesound.org/people/DiArchangeli/packs/22921/ .

All four original recordings are dedicated under **CC0 1.0 Universal**:
https://creativecommons.org/publicdomain/zero/1.0/ .
Full terms and disclaimer: [licenses/CC0-1.0.txt](licenses/CC0-1.0.txt).

| Recording title | Source | Included source file |
| --- | --- | --- |
| Grandfather Clock Striking 12.aif | https://freesound.org/people/DiArchangeli/sounds/458591/ | `sounds/05-diarchangeli-striking-12-preview.mp3` |
| Grandfather Clock Striking at the 15 Minute Mark.aif | https://freesound.org/people/DiArchangeli/sounds/458589/ | `sounds/06-diarchangeli-quarter-15-preview.mp3` |
| Grandfather Clock Chimes at 30 Minute Mark.aif | https://freesound.org/people/DiArchangeli/sounds/458592/ | `sounds/07-diarchangeli-quarter-30-preview.mp3` |
| Grandfather Clock Striking at 45 Minute Mark.aif | https://freesound.org/people/DiArchangeli/sounds/458590/ | `sounds/08-diarchangeli-quarter-45-preview.mp3` |

The included MP3s are the site's public listening previews, copied without
further changes; they are not the original AIFF downloads. The recording author
reports applying white-noise reduction to the original recordings.

Prepared assets and changes by Ticktack:

| File | Changes |
| --- | --- |
| `sounds/chimes/quarter-15.wav` | Decode/resample to mono PCM16, 44.1 kHz; preserve the complete quarter-hour phrase and recorded decay. |
| `sounds/chimes/quarter-30.wav` | Same processing for the half-hour phrase. |
| `sounds/chimes/quarter-45.wav` | Same processing for the three-quarter-hour phrase. |
| `sounds/chimes/hour-prefix.wav` | Extract the first 15.285 seconds of the hourly recording, convert to mono PCM16 at 44.1 kHz, taper the last 4 ms before the first hour strike. |
| `sounds/chimes/strike.wav` | Extract 32.722–58.000 seconds of the hourly recording, retain the last attack and full decay (including residual resonance from preceding strikes), apply a 3 ms fade-in and 400 ms fade-out, convert to mono PCM16 at 44.1 kHz. |

At playback, Ticktack repeats the extracted strike 1–12 times, approximately
1.585 seconds apart, overlapping the decays, and mixes the result with ticking.
No pitch shifting or spectral separation was applied.

The original recordings remain CC0. Ticktack's own edits, to the extent that
they create additional copyright, are offered under CC BY 4.0 as described in
LICENSE.md. CC0 originals remain independently available under CC0.

## Redistribution checklist

- Include LICENSE.md, this file and the `licenses/` directory with source and
  binary/audio distributions.
- Retain the nightcustard attribution, source link and CC BY 4.0 notice.
- Retain the modification descriptions and identify any further changes you make.
- Do not imply endorsement, add restrictions or apply DRM that prevents exercise
  of the rights granted by the applicable license.
- No NonCommercial recordings are included.

Full license texts in `licenses/` are unmodified copies from the SPDX license
list: https://github.com/spdx/license-list-data/tree/main/text . Canonical
Creative Commons license URLs are given above.
