"""Reproducible unit effect meshes and original synthesized weapon audio."""
from pathlib import Path
import math
import random
import struct
import wave

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "SourceArt" / "StellarEffects"
OUT.mkdir(parents=True, exist_ok=True)

def mesh(name, vertices, faces, uv):
    lines = ["# Centimeters; local +Z is the attack direction"]
    lines += [f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in vertices]
    lines += [f"vt {u:.6f} {v:.6f}" for u, v in uv]
    lines += ["f " + " ".join(f"{i + 1}/{i + 1}" for i in face) for face in faces]
    (OUT / (name + ".obj")).write_text("\n".join(lines) + "\n", encoding="ascii")

vertices, uv, faces = [], [], []
segments, rings = 40, 12
for j in range(rings + 1):
    t = j / rings
    for i in range(segments + 1):
        a = math.tau * i / segments
        r = 0.35 + 49.65 * t
        vertices.append((r * math.cos(a), r * math.sin(a), 100 * t))
        uv.append((i / segments, t))
for j in range(rings):
    for i in range(segments):
        a = j * (segments + 1) + i
        faces += [(a, a + segments + 1, a + 1), (a + 1, a + segments + 1, a + segments + 2)]
mesh("SM_StellarPlume", vertices, faces, uv)

vertices, uv, faces = [], [], []
for j in range(13):
    b = math.tau * j / 12
    for i in range(65):
        a = math.tau * i / 64
        r = 48 + 2 * math.cos(b)
        vertices.append((r * math.cos(a), r * math.sin(a), 2 * math.sin(b)))
        uv.append((i / 64, j / 12))
for j in range(12):
    for i in range(64):
        a = j * 65 + i
        faces += [(a, a + 1, a + 65), (a + 1, a + 66, a + 65)]
mesh("SM_StellarOrbit", vertices, faces, uv)

RATE = 44100
def sound(name, duration, kind, loop=False):
    rng = random.Random(917 + kind)
    samples = []
    filtered = 0.0
    for i in range(int(RATE * duration)):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        filtered = filtered * 0.92 + noise * 0.08
        if kind == 0:  # Flame: rolling low roar with a light hiss.
            s = 2.8 * filtered + 0.20 * noise + 0.12 * math.sin(math.tau * 73 * t)
            s *= 0.80 + 0.20 * math.sin(math.tau * 3 * t)
        elif kind == 1:  # Singularity: two beating low tones and moving air.
            s = 0.45 * math.sin(math.tau * 55 * t) + 0.22 * math.sin(math.tau * 83 * t) + filtered
        elif kind == 2:  # Short laser shot; audible transient without sharp clipping.
            s = (math.sin(math.tau * (1500 * t - 3600 * t * t)) * 0.5 + filtered) * math.exp(-t * 28)
        else:  # Repulsion: a descending low pulse.
            s = (math.sin(math.tau * (180 * t - 190 * t * t)) * 0.6 + 2 * filtered) * math.exp(-t * 10)
        if not loop:
            s *= min(1, t / 0.005) * min(1, (duration - t) / 0.02)
        samples.append(s)
    if loop:
        # Blend the final 20 ms toward the beginning so repeating channels have no click.
        seam = int(RATE * 0.02)
        for i in range(seam):
            t = i / seam
            samples[-seam + i] = samples[-seam + i] * (1 - t) + samples[i] * t
    peak = max(abs(s) for s in samples)
    with wave.open(str(OUT / (name + ".wav")), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(b"".join(struct.pack("<h", int(s / peak * 22000)) for s in samples))

sound("S_StellarFlameLoop", 1.0, 0, True)
sound("S_StellarVoidLoop", 1.0, 1, True)
sound("S_StellarFocusShot", 0.14, 2)
sound("S_StellarRepulsion", 0.35, 3)
print("STELLAR_COMBAT_SOURCES", OUT)
