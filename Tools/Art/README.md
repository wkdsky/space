# Art production tools

Keep reusable asset generation, import and configuration scripts here. Disposable screenshots, probes and test reports belong in `Saved/Art/FrontEnd`.

- `frontend_build_blender.py`, `frontend_graphics_blender.py`: source geometry and graphic assets.
- `frontend_animate_blender.py`, `frontend_pose_polish_blender.py`: animation generation and idle pose adjustments.
- `frontend_asset_manifest.py`: source inventory and hashes.
- `frontend_art_mcp.py`: project Unreal MCP entry points (`import`, `lab`, `configure`).
- `frontend_configure_unreal.py`: dispatches the step in `Saved/FrontEndJob.json`: `ui`, `entry`, `lobby`, `player`, `daylight_assets`, `presentation_polish` or `avatar_interaction`.
- `frontend_build_ui_unreal.py`, `frontend_appearance_polish_unreal.py`: widget composition and appearance controls.
- `frontend_avatar_interaction_unreal.py`: direct character interaction, editor-authored bone regions, nameplate styling and hover material. Re-running it updates the existing widgets without rebuilding unrelated pages.
- `frontend_maps_unreal.py`: front-end map configuration.

Use `SourceArt/FrontEnd/asset_manifest.json` as the source inventory. `authoring_only` dependencies are not imported into runtime content. Review each production script before running it because generation, import and configuration steps write assets.
