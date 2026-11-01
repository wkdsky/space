"""Label Blender renders and encode the four-creature animation review reel."""
from pathlib import Path
import subprocess
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[2]/"SourceArt/MoonCreaturesV3/Previews"
SAVED=Path(__file__).resolve().parents[2]/"Saved/MoonCreaturesV3"
FONT=Path("C:/Windows/Fonts/msyh.ttc")
NAMES=("镜虫  Mirror Skimmer","裂唇  Riftlip","塔工蚁  Spire Worker","塔兵蚁  Spire Soldier")
NOTES=("圆角薄瓷板  ·  16 段动作","低矮长楔 / V 形口瓣  ·  17 段动作",
       "短粗陶壶 / 铲形前肢  ·  19 段动作","重型盾头 / 短厚兵颚  ·  21 段动作")
SECTIONS=("待机观察","步行","快速移动","攻击 / 防御反应","贴地采食", "钻入地面","钻出地面","受击反馈","特有动作 I","特有动作 II")
EXTRA=(("晒光调整","表面探查","抹墙修补","低头冲撞"),("侧翻恢复","窜入裂隙","搬运食物","侧击失衡"))


def font(size):
    return ImageFont.truetype(str(FONT),size)


def card(source,section=None,progress=0):
    canvas=Image.new("RGB",(1000,960),(20,29,42))
    canvas.paste(source.convert("RGB").resize((1000,820),Image.Resampling.LANCZOS),(0,80))
    draw=ImageDraw.Draw(canvas)
    draw.text((28,17),"月球生物  ·  模型与动作",font=font(29),fill=(240,240,231))
    draw.text((704,30),"4 种生物  /  73 段骨骼动作",font=font(16),fill=(182,194,207))
    for i,(x,y) in enumerate(((26,94),(526,94),(26,504),(526,504))):
        draw.text((x,y),NAMES[i],font=font(23),fill=(244,244,232),stroke_width=1,stroke_fill=(25,36,50))
        note=NOTES[i]
        if section is not None and section>=8:
            note=EXTRA[section-8][i]
        elif section==3 and i==0:
            note="温和生物的防御轻顶"
        draw.text((x,y+33),note,font=font(16),fill=(229,234,239),stroke_width=1,stroke_fill=(35,47,63))
    title="Blender 源文件  ·  骨骼网格 FBX  ·  独立动画 FBX / GLB"
    if section is not None:
        title=f"{section+1:02d}   {SECTIONS[section]}"
    draw.text((28,913),title,font=font(20),fill=(235,235,222))
    if section is not None:
        draw.text((803,917),"动作预览 / 20 秒",font=font(15),fill=(166,181,198))
        draw.rectangle((0,953,1000,959),fill=(48,61,78))
        draw.rectangle((0,953,round(1000*progress),959),fill=(218,201,166))
    return canvas


def package():
    frames=sorted((SAVED/"ReelRaw").glob("frame_*.png"))
    assert len(frames)==360, f"Expected 360 review frames, got {len(frames)}"
    packed=SAVED/"ReelLabeled";packed.mkdir(parents=True,exist_ok=True)
    for i,path in enumerate(frames):
        with Image.open(path) as img:
            card(img,i//36,(i+1)/360).save(packed/path.name)
    subprocess.run(["ffmpeg","-y","-hide_banner","-loglevel","error","-framerate","18",
        "-i",str(packed/"frame_%04d.png"),"-c:v","libx264","-crf","20","-pix_fmt","yuv420p",
        "-movflags","+faststart",str(ROOT/"MoonCreaturesV3_Actions.mp4")],check=True)
    with Image.open(ROOT/"Gallery.png") as img:
        card(img).save(ROOT/"MoonCreaturesV3_Overview.png")
    print(ROOT/"MoonCreaturesV3_Actions.mp4")
    print(ROOT/"MoonCreaturesV3_Overview.png")


def package_riftlip():
    before=SAVED/"RiftlipRevision_20261031/Before_Head.png"
    comparison=Image.new("RGB",(1600,832),(20,29,42))
    draw=ImageDraw.Draw(comparison)
    draw.text((28,17),"裂唇  /  头部改进对比",font=font(30),fill=(240,240,231))
    for i,(path,title) in enumerate(((before,"调整前"),(ROOT/"Riftlip_Head.png","改进后"))):
        with Image.open(path) as img:
            comparison.paste(img.convert("RGB").resize((800,640),Image.Resampling.LANCZOS),(i*800,100))
        draw.text((28+i*800,65),title,font=font(23),fill=(223,225,217))
    draw.text((28,763),"平直口瓣，中央缝隙较弱",font=font(22),fill=(180,193,206))
    draw.text((828,763),"深 V 裂口 · 厚唇缘 · 内凹密封面",font=font(22),fill=(234,222,194))
    comparison.save(ROOT/"Riftlip_Head_Comparison.png")

    raw=sorted((SAVED/"RiftlipCloseupRaw").glob("frame_*.png"))
    assert len(raw)==240, f"Expected 240 Riftlip frames, got {len(raw)}"
    packed=SAVED/"RiftlipCloseupLabeled";packed.mkdir(exist_ok=True)
    titles=("01  闭合与微动","02  探查表面：张开、贴合","03  摄食：密合、短距离刮取","04  防御：短暂张口与夹持")
    for i,path in enumerate(raw):
        card=Image.new("RGB",(1000,920),(20,29,42))
        with Image.open(path) as img:card.paste(img.convert("RGB"),(0,70))
        draw=ImageDraw.Draw(card)
        draw.text((25,16),"裂唇  /  口部骨骼动画",font=font(28),fill=(240,240,231))
        draw.text((25,880),titles[i//60],font=font(21),fill=(234,222,194))
        draw.rectangle((0,916,round(1000*(i+1)/240),919),fill=(218,201,166))
        card.save(packed/path.name)
    subprocess.run(["ffmpeg","-y","-hide_banner","-loglevel","error","-framerate","20",
        "-i",str(packed/"frame_%04d.png"),"-c:v","libx264","-crf","19","-pix_fmt","yuv420p",
        "-movflags","+faststart",str(ROOT/"Riftlip_Mouth_Actions.mp4")],check=True)
    print(ROOT/"Riftlip_Head_Comparison.png")
    print(ROOT/"Riftlip_Mouth_Actions.mp4")


if __name__=="__main__":
    import argparse
    parser=argparse.ArgumentParser()
    parser.add_argument("--riftlip",action="store_true")
    args=parser.parse_args()
    package_riftlip() if args.riftlip else package()
