"""Create original short prototype gun and impact sounds for Unreal import."""
import math
import random
import struct
import wave
from pathlib import Path

RATE = 44100
OUTPUT = Path(__file__).resolve().parents[2] / "SourceAssets" / "Audio"
OUTPUT.mkdir(parents=True, exist_ok=True)


def make(name, duration, seed, boom_frequency, boom_decay, snap_decay, tail=0.0):
    rng = random.Random(seed)
    count = int(duration * RATE)
    samples = []
    smoothed = 0.0
    for index in range(count):
        t = index / RATE
        noise = rng.uniform(-1.0, 1.0)
        smoothed = smoothed * 0.84 + noise * 0.16
        snap = noise * math.exp(-t * snap_decay)
        bass = math.sin(2.0 * math.pi * (boom_frequency * t - 48.0 * t * t)) * math.exp(-t * boom_decay)
        body = smoothed * math.exp(-t * (boom_decay * 1.45))
        click_t = t - 0.055
        click = (rng.uniform(-1.0, 1.0) * math.exp(-click_t * 190.0)
                 if click_t >= 0 else 0.0)
        value = 0.58 * snap + 0.48 * bass + 0.20 * body + 0.13 * click
        if tail:
            value += tail * math.sin(2.0 * math.pi * (85.0 * t + 25.0 * t * t)) * math.exp(-t * 9.0)
        value *= min(1.0, t * 9000.0)
        samples.append(max(-1.0, min(1.0, value)) * 0.78)
    with wave.open(str(OUTPUT / f"{name}.wav"), "wb") as audio:
        audio.setnchannels(1)
        audio.setsampwidth(2)
        audio.setframerate(RATE)
        audio.writeframes(b"".join(struct.pack("<h", round(sample * 32767)) for sample in samples))


make("S_JTSPistolFire", 0.20, 101, 205, 27, 120)
make("S_JTSMachineGunFire", 0.15, 211, 155, 34, 142)
make("S_JTSSniperFire", 0.38, 307, 112, 12, 82, tail=0.15)
make("S_JTSBulletImpact", 0.16, 419, 285, 38, 170)
print("Generated", len(list(OUTPUT.glob("*.wav"))), "WAV files in", OUTPUT)
