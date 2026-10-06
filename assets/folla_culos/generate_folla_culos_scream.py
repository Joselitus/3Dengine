#!/usr/bin/env python3
"""Generates folla_culos_scream.wav: an agonizing scream of the night creature.

Usage: python3 generate_folla_culos_scream.py       (writes next to this script)
Needs numpy and scipy (scipy.signal.lfilter, for the vocal tract). Fixed seed: the same file every time.
16 bit, mono, 44.1 kHz, about 4.6 s. Nothing in the engine plays it yet.

It is a voice made from scratch (a source and a vocal tract):
  - the source: a train of glottal pulses (a sum of harmonics that falls 12 dB per octave) whose
    pitch follows a curve: a hoarse attack, a rise to a shriek (~900 Hz), a wavering plateau, then
    the voice breaks (it jumps down an octave and back, as an overstrained voice does), and falls
    away into a low rattle; jitter on the pitch and amplitude shaking (shimmer), a subharmonic an
    octave below (the growl of a torn voice) and a fast amplitude tremor;
  - aspiration noise that grows as the voice fails, and a few breaths in between;
  - the vocal tract: four formant resonators that move from an open "aah" (a wide open mouth) to
    a closed, strangled "uuh" at the end;
  - saturation (tanh) for the rasp, a high-pass so that it does not rumble, and a short, dark
    reverb tail, as if in a cave.
Measured, not listened to: see check() (pitch track, loudness and spectrum).
"""
import os
import wave

import numpy as np
from scipy.signal import lfilter

SR = 44100
DURATION = 4.6
OUT = os.path.dirname(os.path.abspath(__file__))
rng = np.random.RandomState(5)
N = int(SR * DURATION)
t = np.arange(N) / SR


def curve(points, kind='smooth'):
    """A curve through (time, value) points, smoothly (cosine) between them."""
    xs = np.array([p[0] for p in points], float)
    ys = np.array([p[1] for p in points], float)
    out = np.interp(t, xs, ys)
    if kind == 'smooth':
        idx = np.clip(np.searchsorted(xs, t) - 1, 0, len(xs) - 2)
        u = np.clip((t - xs[idx]) / (xs[idx + 1] - xs[idx]), 0, 1)
        u = 0.5 - 0.5 * np.cos(np.pi * u)
        out = ys[idx] * (1 - u) + ys[idx + 1] * u
    return out


def lowpass_noise(rate_hz, size=N):
    """Slow random wobble: noise at `rate_hz` interpolated up to the sample rate, about -1..1."""
    n = int(size / SR * rate_hz) + 3
    x = rng.randn(n)
    return np.interp(np.arange(size) / SR * rate_hz, np.arange(n), x) / 2.0


# ----------------------------------------------------------------------------- the pitch
# f0 in Hz over time: the attack, the shriek, the plateau, the break, the fall
f0 = curve([(0.00, 260), (0.10, 330), (0.55, 760), (0.80, 930), (1.30, 880), (1.80, 960),
            (2.15, 700), (2.40, 360), (2.60, 520), (2.90, 300), (3.40, 210), (3.90, 150), (4.60, 95)])
# vibrato, fast and uneven, wider as the voice fails
f0 *= 1.0 + (0.012 + 0.03 * np.clip((t - 1.5) / 3.0, 0, 1)) * np.sin(2 * np.pi * (6.3 + 1.5 * lowpass_noise(2.0)) * t)
f0 *= 1.0 + 0.035 * lowpass_noise(30.0)               # jitter: the cords never repeat exactly
# the voice breaks: sudden drops by an octave and back (the register jumps of a screamed voice)
for start, length in ((1.95, 0.22), (2.55, 0.14), (3.15, 0.30), (3.62, 0.18)):
    gate = np.clip(np.minimum(t - start, start + length - t) / 0.02, 0, 1)
    f0 *= 1.0 - 0.5 * gate
f0 = np.maximum(f0, 40.0)

# ----------------------------------------------------------------------------- the source
phase = 2 * np.pi * np.cumsum(f0) / SR
nyquist = SR / 2
voice = np.zeros(N)
H = 60
for h in range(1, H + 1):
    amp = 1.0 / h ** 1.15                               # a harsh, bright source
    ok = (h * f0) < nyquist * 0.9                       # no aliasing: only the harmonics that fit
    voice += np.where(ok, amp * np.sin(h * phase + 0.4 * h * h * 0.003), 0.0)
# the subharmonic (an octave below): the growl of a torn voice, strongest in the breaks
sub_amount = 0.35 + 0.4 * np.clip((t - 1.8) / 2.0, 0, 1)
voice += sub_amount * np.sin(0.5 * phase) * 0.8 + 0.5 * sub_amount * np.sin(1.5 * phase) * 0.5
# the cords flap: a tremor in the amplitude (30-70 Hz) that makes it rattle
tremor = 1.0 - (0.25 + 0.3 * np.clip((t - 1.5) / 2.5, 0, 1)) * (0.5 + 0.5 * np.sin(2 * np.pi * (46 + 20 * lowpass_noise(3.0)) * t))
shimmer = 1.0 + 0.18 * lowpass_noise(40.0)
voice *= tremor * shimmer
# the aspiration noise: air through a damaged throat, more and more as the voice fails
air = rng.randn(N)
air = lfilter([1.0, -0.6], [1.0], air)                  # tilted towards the highs
air_amount = curve([(0.0, 0.10), (0.5, 0.12), (1.8, 0.2), (2.4, 0.5), (3.2, 0.8), (4.6, 1.0)])
source = voice / np.max(np.abs(voice)) + air_amount * 0.55 * air / np.max(np.abs(air))

# ----------------------------------------------------------------------------- the loudness
attack = np.clip(t / 0.06, 0, 1) ** 1.5
breaths = np.ones(N)
for start, length in ((2.20, 0.10), (2.82, 0.12), (3.55, 0.10), (4.0, 0.14)):   # it runs out of breath
    gate = np.clip(np.minimum(t - start, start + length - t) / 0.03, 0, 1)
    breaths *= 1.0 - 0.8 * gate
body = curve([(0.0, 0.0), (0.06, 0.8), (0.5, 1.0), (1.8, 1.0), (2.4, 0.85), (3.4, 0.55), (4.1, 0.28), (4.6, 0.0)])
loud = attack * body * breaths * (1.0 + 0.2 * lowpass_noise(8.0))
loud = np.clip(loud, 0, None)
# the volume also bursts with the pitch: the high shriek is the loudest
loud *= 0.7 + 0.3 * np.clip((f0 - 250) / 650, 0, 1)

# ----------------------------------------------------------------------------- the vocal tract
# four formants (Hz) that go from a wide open "aah" to a closed, strangled "uuh", with bandwidths
F1 = curve([(0.0, 650), (0.6, 880), (1.8, 850), (2.6, 700), (3.4, 520), (4.6, 330)])
F2 = curve([(0.0, 1250), (0.6, 1400), (1.8, 1350), (2.6, 1150), (3.4, 950), (4.6, 800)])
F3 = curve([(0.0, 2600), (1.8, 2700), (4.6, 2300)])
F4 = np.full(N, 3600.0)
BW = (110.0, 130.0, 190.0, 260.0)
GAINS = (1.0, 0.8, 0.5, 0.3)


def resonate(x, freqs, bw, block=256):
    """A resonator whose centre frequency moves: filtered in blocks (the state is carried over)."""
    y = np.zeros_like(x)
    zi = np.zeros(2)
    for s in range(0, N, block):
        e = min(s + block, N)
        f = float(np.mean(freqs[s:e]))
        r = np.exp(-np.pi * bw / SR)
        a = [1.0, -2.0 * r * np.cos(2 * np.pi * f / SR), r * r]
        b = [1.0 - r]
        seg, zi = lfilter(b, a, x[s:e], zi=zi)
        y[s:e] = seg
    return y


tract = sum(g * resonate(source, F, bw) for g, F, bw in zip(GAINS, (F1, F2, F3, F4), BW))
scream = tract * loud

# ----------------------------------------------------------------------------- the rasp
scream /= np.max(np.abs(scream))
scream = np.tanh(3.2 * scream) / np.tanh(3.2)           # saturation: the throat tearing
# a high-pass at ~90 Hz (no rumble) and a little shelf of the highs for bite
scream = lfilter([1.0, -1.0], [1.0, -0.9873], scream)
scream += 0.35 * lfilter([1.0, -1.0], [1.0, -0.2], scream)

# ----------------------------------------------------------------------------- the cave
tail = np.zeros(N)
for delay, gain in ((0.045, 0.34), (0.083, 0.27), (0.131, 0.2), (0.197, 0.14), (0.281, 0.1)):
    d = int(delay * SR)
    tail[d:] += gain * scream[:N - d]
tail = lfilter([0.18], [1.0, -0.82], tail)              # dark: the echoes lose their highs
scream = scream + 0.8 * tail

# a fade at the very end so that it does not click, and the level (-1 dB peak)
scream *= np.clip((DURATION - t) / 0.05, 0, 1)
scream = scream / np.max(np.abs(scream)) * 0.89


def write(path, x):
    pcm = np.clip(x, -1.0, 1.0)
    pcm = (pcm * 32767.0).astype('<i2')
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())


def check(x):
    """What to trust instead of the ears: pitch, loudness and spectrum over time."""
    print('length %.2f s, peak %.2f, clipped samples: %d' % (len(x) / SR, np.max(np.abs(x)), int(np.sum(np.abs(x) > 0.999))))
    print('rms per 0.5 s (dBFS):', ' '.join('%.0f' % (20 * np.log10(np.sqrt(np.mean(x[int(a * SR):int((a + 0.5) * SR)] ** 2)) + 1e-9))
                                            for a in np.arange(0, DURATION - 0.4, 0.5)))
    print('intended pitch (Hz) every 0.5 s:', ' '.join('%.0f' % f0[int(a * SR)] for a in np.arange(0, DURATION - 0.1, 0.5)))
    win = 4096
    for a in (0.8, 1.8, 3.0, 4.1):
        seg = x[int(a * SR):int(a * SR) + win] * np.hanning(win)
        spec = np.abs(np.fft.rfft(seg))
        freqs = np.fft.rfftfreq(win, 1 / SR)
        total = spec.sum() + 1e-9
        centroid = (spec * freqs).sum() / total
        low = spec[freqs < 500].sum() / total
        high = spec[freqs > 4000].sum() / total
        print('at %.1f s: spectral centroid %4.0f Hz, share below 500 Hz %2.0f %%, above 4 kHz %2.0f %%' % (a, centroid, 100 * low, 100 * high))


if __name__ == '__main__':
    check(scream)
    write(os.path.join(OUT, 'folla_culos_scream.wav'), scream)
    print('written', os.path.join(OUT, 'folla_culos_scream.wav'))
