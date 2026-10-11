"""Makes the fake shade's scream (Shade.cpp: shade_scream.wav, mono, played from where it is) out of
the user's recording of Bob's scream (../bob/bob_scream.wav): its loudest stretch (6.0 s to the end),
in mono, a little lower (x0.85) with a voice an octave and a bit lower under it (x0.6), slightly
clipped, faded at the end: ~5.5 s, the length of the chase. Needs numpy."""

import wave

import numpy as np

RATE = 44100
LENGTH = 5.5  # s

with wave.open('../bob/bob_scream.wav') as w:
    raw = np.frombuffer(w.readframes(w.getnframes()), np.int16)
    x = raw.reshape(-1, w.getnchannels()).mean(1) / 32768.0
x = x[int(6.0 * RATE):]


def lower(signal, factor):
    # slower and lower: played back `factor` times as fast (resampled up by 1/factor)
    t = np.arange(0.0, len(signal) - 1, factor)
    return np.interp(t, np.arange(len(signal)), signal)


main = lower(x, 0.85)
deep = lower(x, 0.6)
n = int(LENGTH * RATE)
out = np.zeros(n)
out[:min(n, len(main))] += main[:n]
out[:min(n, len(deep))] += 0.55 * deep[:n]
out /= np.max(np.abs(out))
out = np.tanh(out * 2.2) / np.tanh(2.2)       # (a rasp)
attack, fade = int(0.02 * RATE), int(0.5 * RATE)
out[:attack] *= np.linspace(0, 1, attack)
out[-fade:] *= np.linspace(1, 0, fade)
out *= 0.89 / np.max(np.abs(out))
with wave.open('shade_scream.wav', 'w') as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(RATE)
    w.writeframes((out * 32767).astype(np.int16).tobytes())
print('shade_scream.wav', round(n / RATE, 2), 's')
