"""Generate unit polar-UV meshes and original deterministic ability sounds (centimeters)."""
from pathlib import Path
import math
import random
import struct
import wave

OUT = Path(__file__).resolve().parents[2] / "SourceArt" / "StellarEffects"
OUT.mkdir(parents=True, exist_ok=True)

for name, inner in (("SM_StellarAbilityDisc", .015), ("SM_StellarAbilityArc", .72)):
    vertices, uv, faces = [], [], []
    segments = 96
    for j in range(9):
        radius = 50 * (inner + (1 - inner) * j / 8)
        for i in range(segments + 1):
            angle = math.tau * (i / segments - .5)
            vertices.append((radius * math.cos(angle), radius * math.sin(angle), 0))
            uv.append((i / segments, inner + (1 - inner) * j / 8))
    for j in range(8):
        for i in range(segments):
            a = j * (segments + 1) + i
            faces += [(a, a + 1, a + segments + 1), (a + 1, a + segments + 2, a + segments + 1)]
    lines = ["# Unit radius 50 cm; +Z is surface up; polar UV"]
    lines += [f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in vertices]
    lines += [f"vt {u:.6f} {v:.6f}" for u, v in uv]
    lines += ["f " + " ".join(f"{i+1}/{i+1}" for i in face) for face in faces]
    (OUT / f"{name}.obj").write_text("\n".join(lines) + "\n", encoding="ascii")

specs = {
    "Explosion": (90, .7), "Healing": (520, 1.0), "Freezing": (900, .22),
    "Shaping": (380, .32), "Radiance": (660, .7), "Diffusion": (140, .18),
    "Shadow": (85, .9), "Instance": (760, .2), "Disassembly": (240, .8),
}
for index, (name, (frequency, duration)) in enumerate(specs.items()):
    rng = random.Random(440 + index)
    samples = []
    rate = 24000
    for i in range(int(duration * rate)):
        t = i / rate
        attack = min(1, t / .018)
        envelope = attack * (1 - t / duration) ** 1.5
        tone = math.sin(math.tau * (frequency * t + frequency * .1 * t * t))
        noise = rng.uniform(-1, 1)
        value = (tone * .25 + noise * (.3 if name in ("Explosion", "Freezing", "Shaping") else .06)) * envelope
        samples.append(int(max(-1, min(1, value)) * 32767))
    with wave.open(str(OUT / f"S_Stellar{name}.wav"), "wb") as wav:
        wav.setnchannels(1); wav.setsampwidth(2); wav.setframerate(rate)
        wav.writeframes(struct.pack("<" + "h" * len(samples), *samples))
print("STELLAR_ABILITY_SOURCES_GENERATED")
