"""Create short, original punch cues for the Space player Blueprint."""

from pathlib import Path
import math
import random
import struct
import wave


RATE = 48000
OUTPUT = Path(__file__).resolve().parents[2] / "SourceAssets" / "Audio"
OUTPUT.mkdir(parents=True, exist_ok=True)


def save(name, samples):
    peak = max(abs(sample) for sample in samples)
    scale = 0.88 / max(peak, 1.0)
    pcm = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, sample * scale)) * 32767)) for sample in samples)
    with wave.open(str(OUTPUT / name), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(pcm)


random.seed(4815)
swing = []
low = 0.0
high = 0.0
for index in range(int(RATE * 0.14)):
    t = index / RATE
    white = random.uniform(-1.0, 1.0)
    low += 0.16 * (white - low)
    high += 0.012 * (white - high)
    band = low - high
    attack = min(1.0, t / 0.045)
    release = max(0.0, 1.0 - max(0.0, t - 0.058) / 0.082)
    sweep = math.sin(2.0 * math.pi * (180.0 * t + 1150.0 * t * t))
    swing.append((band * 0.67 + sweep * 0.10) * attack * release)
save("S_JTSPunchSwing.wav", swing)


random.seed(9173)
impact = []
low = 0.0
phase = 0.0
for index in range(int(RATE * 0.18)):
    t = index / RATE
    white = random.uniform(-1.0, 1.0)
    low += 0.32 * (white - low)
    crack = low * math.exp(-t * 85.0)
    click = white * math.exp(-t * 290.0)
    phase += 2.0 * math.pi * (145.0 - 72.0 * min(1.0, t / 0.14)) / RATE
    thud = math.sin(phase) * (1.0 - math.exp(-t * 240.0)) * math.exp(-t * 25.0)
    impact.append(crack * 0.75 + click * 0.18 + thud * 0.62)
save("S_JTSPunchImpact.wav", impact)
