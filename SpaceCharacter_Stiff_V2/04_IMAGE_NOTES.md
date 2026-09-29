# 图像文件与制作说明

## 规范图与外观图

- `designs/01_walk_technical.png` / `.svg`：走路四相位，正面与右侧视。
- `designs/02_run_technical.png` / `.svg`：跑步四相位，正面与右侧视。
- `designs/03_upper_technical.png` / `.svg`：空手待机、走路、跑步上半身。
- `designs/04_punch_technical.png` / `.svg`：拳击四相位，俯视与正面。
- `Pose_Atlas_V2.pdf`：以上四张技术图的图册。
- `previews/walk_loop.gif`、`run_loop.gif`、`punch_loop.gif`：同坐标系统的循环预览。
- `designs/00_appearance_reference.png`：内置 imagegen 生成、再依据技术图修订的便装角色外观参考。它用于理解轮廓，不定义骨长或精确姿态。

技术图采用程序化投影生成，其源文件在 `source/build_pose_sheets.py`；没有用图像生成模型绘制数字、关节坐标和左右相位。图中左右颜色与数据一致。中文动作说明在 `02_POSE_SPEC_CN.md`，图上使用固定的英文视角和相位代码，便于逐项核对。

外观图中跑步前臂的透视缩短、拳击高角度视角、鞋子的形状均不可当成技术坐标。外观图和技术图存在差异时，以技术图、参数及真实骨骼求解为准。

## 外观图最终修订 Prompt

输入：第一张为已生成的三栏外观草图；第二至四张分别为走路、跑步、拳击技术图。以下是最终局部修订指令（原文）：

```text
Correct the poses in Image1 using the EXACT technical silhouettes from Images2-4. Preserve character, colors, three panel design and Chinese headings. LEFT panel: side view facing screen RIGHT, upright torso, both arms hanging BELOW waist with almost straight elbows, one gently forward and one gently backward only 22deg from downward vertical, never held horizontally. Small uncrossed walking stride as image2 W0 side view, keep legs in separate hip-width lanes. CENTER panel: front view as image3 R1 FRONT. Both hips lowered a little, stance left leg (viewer right) BENT outward moderately (not straight locked); right leg (viewer left) knee bends OUT to side and foot folds inward but stays on its own side. Crucially UPPER ARMS hang DOWNWARD and slightly out only 26 degrees, ELBOWS beside waist/lower ribs, BOTH FOREARMS project FORWARD horizontally with fixed 95deg elbow. Do NOT raise elbows to shoulder height, do not hang forearms vertically down. RIGHT panel: directly overhead TOP VIEW as image4 B0, looking DOWN onto top of head and shoulders, character facing straight toward TOP of panel. Left arm (viewer right) extends straight towards top, right elbow bent out to viewer left and fist halfway toward top. Both arms remain horizontal at chest height in 3D, torso square and motionless, no twist, no boxer guard. Forearms and fists separated in two lanes. No perspective athletic boxing. No arrows or extra labels. These corrective directions override previous art poses.
```
