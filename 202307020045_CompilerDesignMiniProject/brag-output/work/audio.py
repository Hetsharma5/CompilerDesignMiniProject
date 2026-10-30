# Synthesizes music + SFX for the video as one mix -> audio.wav (44.1k stereo)
import json, numpy as np
from scipy.signal import butter, sosfilt, fftconvolve
from scipy.io import wavfile

SR, DUR = 44100, 63.0
N = int(SR * DUR)
rng = np.random.default_rng(7)
mus = np.zeros((N, 2)); sfx = np.zeros((N, 2)); verb_send = np.zeros((N, 2))

def hz(m): return 440 * 2 ** ((m - 69) / 12)
def env_ad(n, a, d):  # attack (s), exponential decay time-constant (s)
    t = np.arange(n) / SR
    return np.minimum(t / max(a, 1e-4), 1) * np.exp(-t / d)
def add(buf, t0, sig, gain=1.0, pan=0.0, send=0.0):
    i = int(t0 * SR)
    if i >= N or i + len(sig) <= 0: return
    s = sig[: N - i] * gain
    L, R = np.cos((pan + 1) * np.pi / 4), np.sin((pan + 1) * np.pi / 4)
    buf[i:i + len(s), 0] += s * L * 1.41; buf[i:i + len(s), 1] += s * R * 1.41
    if send:
        verb_send[i:i + len(s), 0] += s * L * send; verb_send[i:i + len(s), 1] += s * R * send
def lp(x, f, o=2): return sosfilt(butter(o, f, "low", fs=SR, output="sos"), x)
def hp(x, f, o=2): return sosfilt(butter(o, f, "high", fs=SR, output="sos"), x)
def bp(x, lo, hi): return sosfilt(butter(2, [lo, hi], "band", fs=SR, output="sos"), x)

# A minor: Am7 - Fmaj7 - Cmaj7 - G6, one chord per bar (2s at 120 BPM)
CHORDS = [[57, 60, 64, 67], [53, 57, 60, 64], [48, 55, 59, 64], [55, 59, 62, 64]]
ROOTS = [45, 41, 48, 43]
def chord_at(t): return int(t // 2) % 4
BEAT, OUT = 0.5, 56.5

# ---------- music ----------
def saw(f, n, harm=14):
    t = np.arange(n) / SR
    return sum(np.sin(2 * np.pi * f * k * t) / k for k in range(1, harm) if f * k < 9000)
# pad: detuned saws, soft attack, crossfading per bar
for bar in range(int(OUT // 2) + 1):
    t0 = bar * 2.0; n = int(2.6 * SR); c = CHORDS[bar % 4]
    sig = np.zeros(n)
    for m in c:
        for det in (-0.12, 0.12):
            sig += saw(hz(m + det), n)
    sig = lp(sig, 1400) * np.minimum(np.arange(n) / (0.5 * SR), 1) * np.clip((2.6 - np.arange(n) / SR) / 0.6, 0, 1)
    add(mus, t0, sig, 0.022, pan=0, send=0.5)
# kick sidechain ducking curve applied to pad later
kicks = [t for t in np.arange(5.5, OUT, 1.0)] + [t for t in np.arange(11.0, OUT - 0.1, 0.5) if (t - 11) % 1 == 0.5]
kicks = sorted(set(round(k, 3) for k in kicks))
duck = np.ones(N)
for k in kicks:
    i = int(k * SR); n = int(0.35 * SR); d = 1 - 0.55 * np.exp(-np.arange(n) / (0.09 * SR))
    duck[i:i + n] = np.minimum(duck[i:i + n], d[: len(duck[i:i + n])])
mus *= duck[:, None]
# kick
for k in kicks:
    n = int(0.4 * SR); t = np.arange(n) / SR
    f = 45 + 75 * np.exp(-t / 0.04)
    s = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / 0.16)
    s += lp(rng.standard_normal(n), 3000) * np.exp(-t / 0.004) * 0.3
    add(mus, k, s, 0.30 if k < 11 else 0.38)
# clap on 2 & 4, hats on offbeats, from 11s
for b in np.arange(11.0, OUT - 0.1, BEAT):
    beat_in_bar = round((b % 2.0) / BEAT)
    if beat_in_bar in (1, 3):
        n = int(0.25 * SR); s = bp(rng.standard_normal(n), 900, 4000) * env_ad(n, 0.001, 0.06)
        add(mus, b, s, 0.10, send=0.6)
    n = int(0.06 * SR); s = hp(rng.standard_normal(n), 7000) * env_ad(n, 0.001, 0.015)
    add(mus, b + 0.25, s, 0.045, pan=0.3)
# bass: root on each beat with a little movement, from 5.5s
for b in np.arange(5.5, OUT - 0.1, BEAT):
    r = ROOTS[chord_at(b)] - 12; beat_in_bar = round((b % 2.0) / BEAT)
    m = r + (7 if beat_in_bar == 3 and chord_at(b) % 2 else 0)
    n = int(0.48 * SR); t = np.arange(n) / SR
    s = (np.sin(2 * np.pi * hz(m) * t) + 0.25 * np.sin(4 * np.pi * hz(m) * t)) * np.minimum(t / 0.005, 1) * np.exp(-t / 0.35)
    add(mus, b, s, 0.20 if b >= 11 else 0.13)
# arpeggio pluck, 8ths, 11..56.5
def pluck(f, dur=0.35, bright=0.35):
    n = int(dur * SR); t = np.arange(n) / SR
    return (np.sin(2 * np.pi * f * t) + bright * np.sin(4 * np.pi * f * t) * np.exp(-t / 0.05)) * np.minimum(t / 0.002, 1) * np.exp(-t / (dur / 3))
pat = [0, 2, 1, 3, 2, 1, 3, 2]
for j, b in enumerate(np.arange(11.0, OUT - 0.1, 0.25)):
    if j % 2 and b < 39: continue  # 8ths first, 16ths from the code-gen section on
    c = CHORDS[chord_at(b)]
    add(mus, b, pluck(hz(c[pat[j % 8]] + 12), 0.3), 0.045, pan=(-0.4 if j % 2 else 0.4), send=0.35)
# riser into the outro + final Am9 chord and boom
n = int(2.0 * SR); s = rng.standard_normal(n); t = np.arange(n) / SR
s = hp(s, 1500) * (t / 2.0) ** 2.5
add(mus, OUT - 2.0, s, 0.06, send=0.4)
n = int(6.5 * SR); t = np.arange(n) / SR; fin = np.zeros(n)
for m in [45, 52, 57, 60, 64, 67, 71]:
    fin += lp(saw(hz(m), n, 10), 1800) * (0.8 if m < 50 else 1)
fin *= np.minimum(t / 0.05, 1) * np.exp(-t / 6.0)
add(mus, OUT, fin, 0.03, send=0.6)
s = np.sin(2 * np.pi * np.cumsum(40 + 50 * np.exp(-t / 0.06)) / SR) * np.exp(-t / 0.9)
add(mus, OUT, s, 0.28)
for i, m in enumerate([69, 72, 76, 79, 83]):  # sparkling arp that rides the final chord
    add(mus, OUT + 0.08 * i, pluck(hz(m + 12), 1.2, 0.2), 0.05, pan=(-0.5 + 0.25 * i), send=0.7)

# ---------- sfx (in key, sharing the same reverb) ----------
def bell(f, dur=0.8):
    n = int(dur * SR); t = np.arange(n) / SR
    return (np.sin(2 * np.pi * f * t) + 0.3 * np.sin(2 * np.pi * f * 2.76 * t) * np.exp(-t / 0.08)) * np.minimum(t / 0.002, 1) * np.exp(-t / (dur / 4))
def pop(t0, step=0, g=0.07, pan=0.0):
    c = CHORDS[chord_at(t0)]; m = c[step % 4] + 12 * (1 + step // 4)
    add(sfx, t0, bell(hz(m), 0.6), g, pan=pan, send=0.45)
def tick(t0, g=0.05):
    n = int(0.03 * SR); s = hp(rng.standard_normal(n), 3000) * env_ad(n, 0.0005, 0.006)
    add(sfx, t0, s, g, pan=rng.uniform(-.2, .2), send=0.1)
def whoosh(t_end, dur=0.7, g=0.09, up=True):
    n = int(dur * SR); x = rng.standard_normal(n); y = np.zeros(n); t = np.arange(n) / dur / SR
    cut = 300 + 3500 * (t if up else 1 - t) ** 1.5
    a = 1 - np.exp(-2 * np.pi * cut / SR); acc = 0.0
    for i in range(n): acc += a[i] * (x[i] - acc); y[i] = acc
    y *= np.sin(np.pi * t) ** 2
    add(sfx, t_end - dur * 0.65, y, g, send=0.5)
def chime(t0, g=0.06):
    c = CHORDS[chord_at(t0)]
    add(sfx, t0, bell(hz(c[0] + 24), 1.2), g, pan=-0.2, send=0.6)
    add(sfx, t0 + 0.07, bell(hz(c[2] + 24), 1.2), g * 0.8, pan=0.2, send=0.6)
def sparkle(t0, g=0.04):
    c = CHORDS[chord_at(t0)]
    for i, m in enumerate([c[0], c[2], c[0] + 12]):
        add(sfx, t0 + 0.045 * i, bell(hz(m + 24), 0.5), g, pan=-0.3 + 0.3 * i, send=0.6)
def blip_bad(t0, g=0.05):
    n = int(0.18 * SR); t = np.arange(n) / SR
    s = lp(np.sign(np.sin(2 * np.pi * hz(45) * t)), 900) * env_ad(n, 0.003, 0.05)
    add(sfx, t0, s, g, send=0.3)

for k in range(14): tick(0.45 + 0.1 * k, 0.05)                       # S1 typing
for b in [5.5, 11, 20, 30, 39, 48]: whoosh(b, g=0.08)                  # scene changes
for i in range(5): pop(6.3 + 0.35 * i, i, 0.06)                        # S2 cards
for i in range(5): chime(8.7 + 0.3 * i, 0.025) if i == 4 else pop(8.7 + 0.3 * i, i + 4, 0.035)
for i, t in enumerate(json.load(open("toktimes.json"))): pop(t, i, 0.055, pan=-0.6 + 0.17 * i)  # tokens
for i in range(3): blip_bad(16.5 + 0.12 * i, 0.04)                     # lexical errors
for lv, t in enumerate([20.9, 21.6, 22.3, 23.0]): pop(t, lv * 2, 0.065)  # AST levels
chime(25.6, 0.045)                                                     # * binds tighter
for i in range(6): tick(30.9 + 0.25 * i, 0.06)                         # symbol rows
chime(33.0, 0.04); pop(34.0, 2, 0.05); chime(35.5, 0.04)
for i in range(1, 5): blip_bad(36.5 + 0.18 * i, 0.035)                 # semantic errors
for i, t in enumerate([40.4, 41.4, 42.4]): pop(t, i * 2, 0.065)        # TAC lines
pop(43.4, 1, 0.045); pop(44.8, 3, 0.045)
for t in [49.2, 50.2, 50.9, 51.5, 52.1]: sparkle(t, 0.04)              # propagate / fold
whoosh(53.2, 0.6, 0.06, up=False)                                      # dead temps collapse
chime(53.7, 0.045)
x = np.linspace(0, 1, 2000); vals = 35 - np.round(5 * (1 - (1 - x) ** 3))
for j in np.nonzero(np.diff(vals))[0]: tick(53.8 + x[j + 1], 0.07)     # counter
for i in range(5): pop(58.1 + 0.2 * i, i + 2, 0.045, pan=0.3)           # outro checklist

# ---------- reverb + mix ----------
n = int(2.2 * SR); t = np.arange(n) / SR
ir = np.stack([lp(rng.standard_normal(n), 6000) * np.exp(-t / 0.55) for _ in range(2)], 1); ir /= np.abs(ir).sum(0) ** 0.5 * 4
wet = np.stack([fftconvolve(verb_send[:, c], ir[:, c])[:N] for c in range(2)], 1)
mix = mus + sfx * 0.9 + wet * 0.6
mix = np.stack([hp(mix[:, c], 30) for c in range(2)], 1)
fade = np.ones(N); fi = int(62.0 * SR); fade[fi:] = np.linspace(1, 0, N - fi)
mix *= fade[:, None]; mix[: int(0.01 * SR)] *= np.linspace(0, 1, int(0.01 * SR))[:, None]
mix = np.tanh(mix / np.abs(mix).max() * 1.4) ; mix *= 0.89 / np.abs(mix).max()
wavfile.write("audio.wav", SR, (mix * 32767).astype(np.int16))
print("rms dBFS", 20 * np.log10(np.sqrt((mix ** 2).mean())))
