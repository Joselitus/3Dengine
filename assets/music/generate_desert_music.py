#!/usr/bin/env python3
"""Procedurally generates desert.wav: a seamless background loop for the desert.

Usage: python3 generate_desert_music.py        (writes next to this script)
Needs numpy only. Everything is seeded, so the output is reproducible.

An arid, slow piece (75 BPM, 24 bars, 76.8 s) in A with a flamenco / spaghetti-
western flavour: the Andalusian cadence Am - G - F - E, with the melody in
E Phrygian dominant. Voices:
  - a nylon guitar fingerpicking (Karplus-Strong plucked string)
  - a banjo playing the melody, with tremolo on the long notes
  - a low drone (open fifth), a thin wind, and a soft frame drum
The reverb is a circular convolution, so its tail wraps around to the start
and the file loops with no seam. All frequencies in the drone and the wind
make a whole number of cycles over the loop for the same reason.

Structure (bars): 0-3 intro, 4-11 guitar, 12-19 guitar + banjo + drum, 20-23
outro (thins out into the intro again).
"""
import os
import wave
import numpy as np

OUT = os.path.dirname(os.path.abspath(__file__))
SR = 32000                      # sample rate; 76.8 s of stereo 16 bit ~ 9.8 MB
BPM = 75
BEAT = 60.0 / BPM               # 0.8 s
BAR = 4 * BEAT                  # 3.2 s = 102400 samples: a whole number
BARS = 24
N = int(round(BARS * BAR * SR))
LOOP_SECONDS = N / SR

rng = np.random.default_rng(11)


def hz(midi):
    return 440.0 * 2.0 ** ((midi - 69) / 12.0)


# ------------------------------------------------------------------ filters
def fft_filter(x, response):
    """Applies response(freqs) to a whole signal (a circular filter)."""
    spec = np.fft.rfft(x)
    freqs = np.fft.rfftfreq(len(x), 1.0 / SR)
    return np.fft.irfft(spec * response(freqs), len(x))


def bandpass(lo, hi):
    def r(f):
        return (1 / (1 + (lo / np.maximum(f, 1e-3)) ** 4)) * (1 / (1 + (f / hi) ** 4))
    return r


def lowpass(fc, order=4):
    return lambda f: 1.0 / (1.0 + (f / fc) ** order)


def smooth(x, passes):
    """Cheap low-pass: repeated two-point average."""
    for _ in range(passes):
        x = 0.5 * (x + np.roll(x, 1))
    return x


# ------------------------------------------------------- plucked strings
def pluck(freq, dur, noise, t60, pos, soften=0):
    """Karplus-Strong string, one block of a delay line at a time.

    The string starts as it does when it is plucked: a triangular displacement
    with its peak at `pos` (a fraction of the length, nearer the bridge = a
    brighter sound), which gives the fundamental its proper strength; some
    noise is added on top for the pick's roughness.

    freq   Hz
    dur    seconds
    noise  how much noise is added to the triangle (0 = none)
    t60    seconds the string takes to fall by 60 dB
    pos    0..0.5 where the string is plucked
    soften passes of smoothing on the start (darker, like a fingertip on nylon)
    """
    total = int(dur * SR)
    delay = SR / freq - 0.5     # the two-point average adds half a sample
    ni = int(delay)
    fi = delay - ni
    c0, c1, c2 = 0.5 * (1 - fi), 0.5, 0.5 * fi          # fractional delay
    gain = 10.0 ** (-3.0 / (freq * t60))                # per trip round the string

    x = np.arange(ni + 3) / (ni + 3)
    exc = np.where(x < pos, x / pos, (1 - x) / (1 - pos))
    exc = exc - exc.mean()
    rough = rng.uniform(-1, 1, ni + 3)
    rough -= rough.mean()
    exc = exc / np.sqrt((exc ** 2).mean()) + noise * rough / np.sqrt((rough ** 2).mean())
    exc = smooth(exc, soften)
    exc -= exc.mean()
    exc /= max(np.abs(exc).max(), 1e-9)

    y = np.zeros(total + ni + 3)
    y[:ni + 3] = exc
    for s in range(ni + 3, total, ni):
        e = min(s + ni, total)
        y[s:e] = gain * (c0 * y[s - ni:e - ni] + c1 * y[s - ni - 1:e - ni - 1] +
                         c2 * y[s - ni - 2:e - ni - 2])
    y = y[:total]
    # a short fade at the end so a cut note does not click
    fade = min(int(0.02 * SR), total)
    y[total - fade:] *= np.linspace(1, 0, fade)
    return y


def guitar_note(midi, dur, vel):
    f = hz(midi) * 2 ** (rng.normal(0, 3) / 1200)       # a few cents of detune
    return vel * pluck(f, dur, noise=0.35, t60=3.0 if midi < 55 else 2.2, pos=0.2, soften=2)


def banjo_note(midi, dur, vel):
    f = hz(midi) * 2 ** (rng.normal(0, 3) / 1200)
    y = pluck(f, dur, noise=0.9, t60=0.9, pos=0.09, soften=0)
    # the click of the pick on the drum head
    n = int(0.03 * SR)
    click = fft_filter(rng.uniform(-1, 1, n), bandpass(500, 2500))
    y[:n] += 0.9 * click * np.exp(-np.arange(n) / (0.008 * SR))
    return vel * y


# ------------------------------------------------------------- the mix bus
class Stereo:
    def __init__(self):
        self.l = np.zeros(N)
        self.r = np.zeros(N)

    def add(self, sig, start, gain=1.0, pan=0.0):
        """Adds a mono signal at sample `start`, wrapping around the loop."""
        a = (pan + 1) * np.pi / 4                       # equal power pan
        gl, gr = np.cos(a) * gain, np.sin(a) * gain
        start %= N
        k = 0
        while k < len(sig):
            n = min(len(sig) - k, N - start)
            self.l[start:start + n] += gl * sig[k:k + n]
            self.r[start:start + n] += gr * sig[k:k + n]
            k += n
            start = 0


def at(bar, beat):
    """Sample index of a bar (0-based) and a beat inside it (can be fractional)."""
    return int(round((bar * 4 + beat) * BEAT * SR))


def human(seconds=0.010):
    return int(rng.normal(0, seconds) * SR)


# ----------------------------------------------------------------- harmony
# Andalusian cadence, strings from low to high (MIDI note numbers)
VOICING = {
    "Am": [45, 52, 57, 60, 64],
    "G": [43, 50, 55, 59, 62],
    "F": [41, 48, 53, 57, 60],
    "E": [40, 47, 52, 56, 59],
}
PROGRESSION = ["Am", "G", "F", "E"]


def chord_of(bar):
    return PROGRESSION[bar % 4]


# the fingerpicking pattern: one string index per eighth note
PATTERN = [0, 3, 4, 3, 1, 3, 4, 3]
PATTERN_VEL = [1.0, 0.55, 0.65, 0.5, 0.8, 0.55, 0.65, 0.5]

# melody (bar, beat, midi, beats long): bars 12-19, over Am G F E twice
A4, B4, C5, D5, E5, F5, A5 = 69, 71, 72, 74, 76, 77, 81
G4, Gs4 = 67, 68
MELODY = [
    (12, 0.0, E5, 1.5), (12, 1.5, D5, 0.5), (12, 2.0, C5, 1.0), (12, 3.0, A4, 1.0),
    (13, 0.0, B4, 1.5), (13, 1.5, D5, 0.5), (13, 2.0, D5, 1.0), (13, 3.0, B4, 1.0),
    (14, 0.0, C5, 1.5), (14, 1.5, A4, 0.5), (14, 2.0, C5, 1.0), (14, 3.0, F5, 1.0),
    (15, 0.0, Gs4, 1.0), (15, 1.0, B4, 1.0), (15, 2.0, E5, 1.5), (15, 3.5, D5, 0.5),
    (16, 0.0, E5, 1.0), (16, 1.0, A5, 1.5), (16, 2.5, E5, 0.5), (16, 3.0, E5, 1.0),
    (17, 0.0, D5, 1.0), (17, 1.0, B4, 1.0), (17, 2.0, G4, 2.0),
    (18, 0.0, A4, 1.0), (18, 1.0, C5, 1.0), (18, 2.0, F5, 1.0), (18, 3.0, E5, 1.0),
    (19, 0.0, Gs4, 1.0), (19, 1.0, B4, 1.0), (19, 2.0, E5, 2.0),
]


# levels of the voices (the strings lead the mix)
BANJO_GAIN = 1.4
DRUM_GAIN = 0.3
DRONE = 0.045
WIND = 0.2


def render():
    guitar = Stereo()
    banjo = Stereo()
    drums = Stereo()

    # ---- guitar
    for bar in range(BARS):
        v = VOICING[chord_of(bar)]
        if bar < 4 or bar >= 20:
            # intro and outro: a slow roll, one string per beat, soft
            order = [0, 2, 3, 4] if bar % 2 == 0 else [0, 1, 3, 4]
            for beat, s in enumerate(order):
                vel = (0.55 if bar < 4 else 0.45) * (1.0 if beat == 0 else 0.6)
                note = guitar_note(v[s], 3.2, vel * rng.uniform(0.9, 1.1))
                guitar.add(note, at(bar, beat) + human(0.012), 0.55, -0.25)
        else:
            for step, s in enumerate(PATTERN):
                vel = PATTERN_VEL[step] * rng.uniform(0.9, 1.1)
                note = guitar_note(v[s], 2.4, vel)
                guitar.add(note, at(bar, step * 0.5) + human(), 0.55, -0.25)

    # ---- banjo: the melody, with tremolo on the long notes
    for bar, beat, midi, beats in MELODY:
        dur = beats * BEAT
        first = banjo_note(midi, min(dur + 0.8, 2.0), rng.uniform(0.85, 1.0))
        banjo.add(first, at(bar, beat) + human(0.008), BANJO_GAIN, 0.3)
        if beats >= 1.5:
            hits = int(beats / 0.25)                    # a roll of 16ths
            for k in range(1, hits):
                vel = 0.6 * (1 - k / (hits + 2)) * rng.uniform(0.85, 1.0)
                banjo.add(banjo_note(midi, 0.7, vel), at(bar, beat + 0.25 * k) + human(0.006),
                          BANJO_GAIN, 0.3)

    # ---- frame drum: a thump on 1 and a lighter one on 3 (bars 8-19), and a
    # tap on the last "and" of every second bar
    def thump(vel):
        n = int(0.5 * SR)
        t = np.arange(n) / SR
        freq = 60 + 85 * np.exp(-t / 0.05)              # drops from 145 to 60 Hz
        body = np.sin(2 * np.pi * np.cumsum(freq) / SR) * np.exp(-t / 0.13)
        skin = fft_filter(rng.uniform(-1, 1, n), bandpass(900, 3500)) * np.exp(-t / 0.012)
        return vel * (body + 0.25 * skin)

    for bar in range(8, 20):
        drums.add(thump(0.9 if bar >= 12 else 0.5), at(bar, 0) + human(0.004), DRUM_GAIN, 0.0)
        drums.add(thump(0.45 if bar >= 12 else 0.25), at(bar, 2) + human(0.004), DRUM_GAIN, 0.0)
        if bar % 2 == 1 and bar >= 12:
            drums.add(thump(0.3), at(bar, 3.5) + human(0.004), DRUM_GAIN * 0.8, 0.0)

    return guitar, banjo, drums


def ambience():
    """Drone and wind. Every frequency makes a whole number of cycles in the loop."""
    t = np.arange(N) / SR

    def snap(f):
        return round(f * LOOP_SECONDS) / LOOP_SECONDS

    def swell(cycles, phase):
        return 0.5 + 0.5 * np.sin(2 * np.pi * cycles * t / LOOP_SECONDS + phase)

    drone_l = np.zeros(N)
    drone_r = np.zeros(N)
    # A1 and E2 (an open fifth), a little detuned in each ear, with soft overtones
    for base, level in ((55.0, 1.0), (82.4, 0.6), (110.0, 0.4)):
        for k, (side, drift) in enumerate(((drone_l, -0.12), (drone_r, +0.12))):
            f = snap(base + drift)
            tone = (np.sin(2 * np.pi * f * t) + 0.35 * np.sin(2 * np.pi * 2 * f * t + 1.0) +
                    0.12 * np.sin(2 * np.pi * 3 * f * t + 2.0))
            side += level * tone * (0.55 + 0.45 * swell(3 + k, 0.7 * level))
    drone_l = fft_filter(drone_l, lowpass(700))
    drone_r = fft_filter(drone_r, lowpass(700))

    wind = []
    for ear in range(2):
        noise = rng.uniform(-1, 1, N)
        noise = fft_filter(noise, bandpass(180, 1100))
        gust = (0.45 + 0.55 * swell(5 + ear, 0.3 + 2 * ear) * swell(9 - ear, 1.0 + ear)) ** 2
        wind.append(noise * gust)
    return drone_l, drone_r, wind[0], wind[1]


def reverb_ir(seconds=2.4):
    """A stereo room: sparse early reflections and a damped, decaying noise."""
    n = int(seconds * SR)
    t = np.arange(n) / SR
    irs = []
    for _ in range(2):
        tail = fft_filter(rng.normal(0, 1, n), lowpass(4500, 2)) * np.exp(-6.9 * t / 2.2)
        tail[:int(0.02 * SR)] = 0
        early = np.zeros(n)
        for delay in rng.uniform(0.012, 0.09, 7):
            early[int(delay * SR)] += rng.uniform(0.2, 0.5) * rng.choice([-1, 1])
        ir = tail + early
        irs.append(ir / np.sqrt((ir ** 2).sum()))
    return irs


def circular_convolve(x, ir):
    return np.fft.irfft(np.fft.rfft(x) * np.fft.rfft(ir, len(x)), len(x))


def main():
    guitar, banjo, drums = render()
    drone_l, drone_r, wind_l, wind_r = ambience()

    # the voices on their own buses
    def body(x):            # warmth of the guitar's box
        return x + 0.5 * fft_filter(x, bandpass(90, 260))

    def twang(x):           # the banjo's ring
        return x + 0.45 * fft_filter(x, bandpass(1400, 3800))

    dry_l = (body(guitar.l) + twang(banjo.l) + drums.l +
             DRONE * drone_l + WIND * wind_l)
    dry_r = (body(guitar.r) + twang(banjo.r) + drums.r +
             DRONE * drone_r + WIND * wind_r)

    # reverb: the whole mix except the thump goes in, 30% wet
    ir_l, ir_r = reverb_ir()
    send_l = body(guitar.l) + twang(banjo.l) + 0.6 * WIND * wind_l + 0.3 * DRONE * drone_l
    send_r = body(guitar.r) + twang(banjo.r) + 0.6 * WIND * wind_r + 0.3 * DRONE * drone_r
    wet_l = circular_convolve(send_l, ir_l) + 0.3 * circular_convolve(send_r, ir_r)
    wet_r = circular_convolve(send_r, ir_r) + 0.3 * circular_convolve(send_l, ir_l)
    left = dry_l + 0.55 * wet_l
    right = dry_r + 0.55 * wet_r

    # nothing below 35 Hz (it only costs headroom), then a normalised peak
    left = fft_filter(left, lambda f: 1 / (1 + (35 / np.maximum(f, 1e-3)) ** 4))
    right = fft_filter(right, lambda f: 1 / (1 + (35 / np.maximum(f, 1e-3)) ** 4))
    peak = max(np.abs(left).max(), np.abs(right).max())
    left, right = 0.8 * left / peak, 0.8 * right / peak

    pcm = np.empty(2 * N, dtype="<i2")
    pcm[0::2] = np.round(left * 32767)
    pcm[1::2] = np.round(right * 32767)
    path = os.path.join(OUT, "desert.wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())
    print("wrote %s: %.1f s, %d Hz stereo, %.1f MB" %
          (path, LOOP_SECONDS, SR, os.path.getsize(path) / 1e6))


if __name__ == "__main__":
    main()
