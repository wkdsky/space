# Front-end source assets

This directory retains production source files and the manifests needed to rebuild `Content/Space/FrontEnd`.

- `FrontEnd_Models.blend`, `FrontEnd_Threat.blend`: editable Blender source scenes.
- `Characters/`, `Models/`, `Animations/`: exported FBX sources and reusable animation clips.
- `Textures/`, `UI/`, `Audio/`: retained source textures, the expression atlas, icons and audio.
- `Fonts/`: the JTS UI font subset and its license.
- `External/`: source packages required by the maintained production scripts.
- `asset_manifest.json`: source paths and SHA-256 hashes; `authoring_only` entries stay outside runtime content.
- Other `*_manifest.json` files and `sources.json`: import settings and attribution.

Screenshots, inspection dumps and intermediate test reports belong in `Saved/Art/FrontEnd`, not this source directory.

## Rebuild

1. Run the maintained Blender production scripts through `Tools/Art/frontend_blender_client.py` when source geometry or animation changes.
2. Refresh the source manifest with `python Tools/Art/frontend_asset_manifest.py`.
3. Register `Tools/Art/frontend_art_mcp.py` in Unreal Python and run the required import/configure step.
4. `frontend_build_ui_unreal.py` builds the menu and lobby widget composition, including appearance controls and direct avatar interaction.
5. `frontend_appearance_polish_unreal.py` configures appearance controls and the lobby appearance camera. `frontend_avatar_interaction_unreal.py` authors nameplate fonts, hover material and pointer regions.

The canonical runtime font assets are `F_JTS_UI_Regular_Font` and `F_JTS_UI_Bold_Font`. Attribution and licenses are retained in `Content/Space/FrontEnd/Legal`.
