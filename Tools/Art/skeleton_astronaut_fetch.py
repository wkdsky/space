"""Download the CC0 source assets used by the Skeleton Astronaut character."""
import json
import re
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2] / "SourceArt" / "SkeletonAstronaut"
ROOT.mkdir(parents=True, exist_ok=True)

def fetch(url, name):
    target = ROOT / name
    if not target.exists():
        urllib.request.urlretrieve(url, target)
    print(name, target.stat().st_size)
    return target

if __name__ == "__main__":
    base = "https://raw.githubusercontent.com/KayKit-Game-Assets/KayKit-Character-Pack-Skeletons-1.0/main/"
    fetch(base + "LICENSE.txt", "LICENSE_KayKit.txt")
    fetch(base + "addons/kaykit_character_pack_skeletons/Characters/gltf/Skeleton_Warrior.glb", "Skeleton_Warrior.glb")
    fetch("https://drive.usercontent.google.com/download?id=1rlsFDUgALSjeXdL5lGvLLABK_Cb6nLs4&export=download", "Astronaut_RaeTheRedPanda.blend")
    fetch("https://drive.usercontent.google.com/download?id=1Wo82Sbrl8Ql67XpBAwrsCH62MtFMeId3&export=download", "Atlas.png")
    fetch("https://drive.usercontent.google.com/download?id=1WmpH3wsL_759gtn2JQTsz0v4eyG0bK-Z&export=download", "LICENSE_Quaternius.txt")
    if len(sys.argv) > 1:
        page = urllib.request.urlopen("https://drive.google.com/embeddedfolderview?id=" + sys.argv[1]).read().decode()
        print(re.findall(r'id="entry-([^\"]+)".*?class="flip-entry-title">([^<]+)', page))
