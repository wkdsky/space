# Jump to Space — Codex Project Instructions

## 1. Project identity

- Product name: **Jump to Space**
- Unreal project directory/repository: `space`
- Engine: Unreal Engine 5. Respect the exact engine version and module configuration already declared by the project.
- Primary implementation language: **C++** for gameplay systems and reusable core logic.
- Use Blueprints / Data Assets / Data Tables for designer-facing content and presentation when appropriate, but do not move core game rules into Level Blueprints.

The intended high-level gameplay loop is:

1. Start on Earth.
2. The player has limited time to scavenge resources and prepare.
3. When the phase ends, the player launches the spacecraft.
4. The first destination is the Moon.
5. On the Moon, the player gathers resources, repairs the ship, and upgrades it.
6. The player then travels to additional planets.
7. The loop expands through new locations, resources, hazards, ship upgrades, and progression.

Treat this as the product direction, not as permission to implement systems that were not requested.

---

## 2. Autonomy and scope

For requests that ask to implement, fix, refactor, or change code:

- Inspect the relevant project files first.
- Make the required in-scope edits directly.
- Do **not** ask for routine confirmation before creating, editing, renaming, or deleting project source files when the requested task clearly requires it.
- Run relevant non-destructive validation after changes.
- Fix compilation errors caused by your changes when practical.
- Continue until the requested task is complete or a real blocker prevents progress.

For requests that only ask to explain, review, diagnose, or plan:

- Inspect relevant files as needed.
- Do not modify files unless the request also authorizes implementation.

Do not expand the task into unrelated refactors or features.

Never perform the following unless the user explicitly requests it:

- destructive Git operations such as `reset --hard`, `clean -fd`, history rewriting, or forced pushes;
- deleting unrelated project files;
- modifying files outside this repository;
- committing or pushing changes;
- installing unrelated system-wide software;
- changing account credentials, secrets, or machine security settings.

If an external or destructive action is genuinely necessary, stop and report the blocker rather than silently widening scope.

---

## 3. Repository and directory discipline

Honor the directory structure that already exists in this repository.

- Do not invent a parallel folder hierarchy when an appropriate existing folder already exists.
- Before adding a new gameplay system, inspect neighboring systems and follow the repository's established organization.
- Keep related `.h` and `.cpp` files in the appropriate module/folder.
- If a new module is genuinely required, update the relevant `.uproject`, `.Build.cs`, and target configuration carefully and only as needed.
- Avoid unnecessary file moves because Unreal asset/code references can be fragile.

Do not manually edit generated/build-output directories:

- `Binaries/`
- `Intermediate/`
- `DerivedDataCache/`
- `Saved/`
- `.vs/`

Do not treat generated IDE/project files as source of truth.

---

## 4. Unreal Engine C++ rules

Follow Unreal Engine conventions and the style already present in this repository.

### Reflection and UObject safety

- Use `UCLASS`, `USTRUCT`, `UENUM`, `UINTERFACE`, `UPROPERTY`, and `UFUNCTION` correctly when Unreal reflection, serialization, Blueprint exposure, replication, or garbage collection requires them.
- Use `GENERATED_BODY()` correctly.
- In reflected headers, keep the generated header include in the required position.
- Do not allocate `UObject` types with raw `new` / `delete`.
- Use Unreal object creation APIs such as `NewObject`, `CreateDefaultSubobject`, or spawning APIs as appropriate.
- Use `TObjectPtr`, weak references, soft references, or other Unreal-supported pointer types according to ownership/lifetime needs and the engine version used by the project.
- Avoid dangling UObject references and GC-unsafe raw references.

### Includes and compilation

- Prefer forward declarations when practical.
- Keep headers lightweight.
- Add module dependencies to `.Build.cs` only when they are actually required.
- Do not solve include errors by adding broad or unrelated module dependencies.
- Pay attention to Unreal Header Tool constraints.
- Preserve correct public/private dependency boundaries.

### Naming

Follow Unreal naming conventions:

- `A` for Actor-derived classes.
- `U` for UObject-derived classes.
- `F` for structs and non-UObject Unreal-style types where appropriate.
- `E` for enums where appropriate.
- `I` for Unreal interfaces.
- Boolean member names should normally use the `b` prefix.
- Use descriptive gameplay names rather than generic names such as `Manager2`, `Helper`, or `SystemNew`.

Follow existing project naming when it is more specific.

### Runtime design

- Avoid `Tick` unless per-frame work is genuinely necessary.
- Prefer events, delegates, timers, state changes, and explicit gameplay transitions.
- Keep classes focused; do not grow one God Object that owns unrelated systems.
- Prefer composition and components/subsystems when they match the lifetime and responsibility of the feature.
- Do not introduce Gameplay Ability System, Mass, networking/replication, or another large framework unless the current task actually requires it.
- Avoid speculative abstractions for hypothetical future features.

---

## 5. Jump to Space architecture principles

Build toward a maintainable vertical slice before generalizing.

### Game flow

The high-level phase flow should remain explicit and understandable:

`Earth Preparation -> Launch -> Planet Exploration -> Repair/Upgrade -> Next Destination`

Do not hide global game-flow logic inside arbitrary actors or Level Blueprints.

When implementing global progression:

- distinguish level-local state from cross-level/session state;
- use Unreal framework lifetimes intentionally;
- avoid global mutable statics for gameplay state;
- use SaveGame only for data that should persist across play sessions;
- do not persist temporary runtime state unnecessarily.

### Gameplay systems

Prefer data-driven definitions for content that will grow substantially, including candidates such as:

- items/resources;
- resource node definitions;
- planets/destinations;
- ship parts;
- ship upgrades;
- crafting/repair recipes;
- encounter or hazard configuration.

Do not hard-code large content catalogs into C++ when data assets or tables are a better fit.

Keep system boundaries explicit. Inventory, interaction, resources, ship state, progression, crafting/repair, and game-flow logic should not become tightly coupled without a clear reason.

Prefer interfaces, components, delegates, or subsystem APIs where they reduce coupling meaningfully; do not add abstraction purely for abstraction's sake.

---

## 6. Blueprint and asset boundary

`.uasset` and `.umap` files are binary Unreal assets.

By default:

- Do not attempt to hand-edit their binary contents.
- Do not casually rename/move/delete Unreal assets from the filesystem.
- Do not manufacture fake `.uasset` files.
- Do not assume a Blueprint asset exists unless it is present in the repository.

When a requested feature requires Unreal Editor work that cannot be completed safely through source files alone:

1. Implement the required C++ foundation and Blueprint-facing API where appropriate.
2. State exactly what remains to be done in the Unreal Editor.
3. Give concise editor steps, including suggested asset names and destination paths.
4. Do not pretend the editor-side asset was created if it was not.

If explicit Unreal Editor automation is requested and a reliable project-supported automation path exists, it may be used after inspecting the project setup.

---

## 7. Build and validation

After meaningful C++ changes, validate them when the local environment permits.

Before building:

- inspect the `.uproject`;
- inspect `Source/*.Target.cs` and module `.Build.cs` files;
- determine the actual Editor target name instead of guessing;
- discover the installed Unreal Engine path rather than hard-coding another machine's path.

Prefer an Unreal-supported build path such as UnrealBuildTool / `Engine\Build\BatchFiles\Build.bat` or the project's valid generated solution/build tooling.

For example, derive the equivalent of:

`<UE>\Engine\Build\BatchFiles\Build.bat <ProjectEditorTarget> Win64 Development <AbsolutePathToUProject> -WaitMutex`

Do not blindly copy this example if the project target/configuration differs.

Validation order for code changes:

1. Check the edited code for obvious UHT/C++ issues.
2. Build the relevant Editor target when practical.
3. Fix errors introduced by the change.
4. Re-run the build after fixes.
5. Inspect `git diff` for accidental or unrelated modifications.

Do not claim a successful build or test unless it actually completed successfully.

If a build is blocked by the Unreal Editor, Live Coding, missing SDK/toolchain, engine installation, or another environment issue:

- do not kill unrelated user processes without permission;
- report the exact blocker and the command/result that exposed it.

Warnings introduced by the change should be addressed when practical.

---

## 8. Persistent project knowledge

Long chat context is temporary. Important project knowledge must live in the repository.

When relevant documentation exists under `Docs/`, treat it as persistent project memory.

Recommended durable documents are:

- `Docs/Architecture.md` — current system architecture and ownership/lifetime boundaries.
- `Docs/GameFlow.md` — authoritative gameplay loop and phase transitions.
- `Docs/Systems.md` — implemented gameplay systems and their public responsibilities.
- `Docs/Decisions.md` — important architectural decisions and reasons.
- `Docs/CurrentState.md` — concise current vertical-slice status, major blockers, and next milestones.

Rules:

- Read only the documents relevant to the current task; do not load every document reflexively.
- If a substantial implementation changes an architectural decision already documented, update the affected durable document.
- Keep durable docs concise and current.
- Do not use durable docs as a chronological dump of every small code edit.
- Do not preserve obsolete architecture just because an old chat message mentioned it.
- Repository code plus current durable docs are the source of truth.

If these files do not yet exist, do not create all of them merely because they are listed here. Create or initialize them when the task actually needs persistent documentation, or when explicitly requested.

After context compaction, a resumed session, or uncertainty about a prior architectural decision, re-check the relevant durable document before making architecture-changing edits.

---

## 9. Work procedure

For a normal implementation task:

### Before editing

1. Read this project's applicable instructions.
2. Check `git status`.
3. Inspect the smallest relevant set of existing files.
4. Check for more specific nested `AGENTS.md` instructions when working in a nested area.
5. Understand existing patterns before creating new ones.

### During implementation

1. Make a cohesive, minimal change that completes the requested behavior.
2. Reuse existing systems when appropriate.
3. Avoid unrelated formatting churn.
4. Avoid speculative future features.
5. Keep public APIs intentional and small.
6. Add comments for non-obvious intent, not for obvious syntax.

### After implementation

1. Build/test or otherwise validate the change.
2. Fix issues caused by the change when practical.
3. Inspect the final diff.
4. Update durable architecture docs only if the change materially affects them.
5. Report completion clearly.

---

## 10. Final response format

After implementation, keep the final response concise but concrete.

Include:

- what was implemented;
- important files changed;
- validation/build result;
- any Unreal Editor steps the user still needs to perform;
- genuine blockers or known limitations.

Do not provide a long narration of every terminal command.

Do not say something is complete if required code is missing or compilation is known to fail.

---

## 11. Priority rules

When instructions conflict, follow the higher-priority instruction source provided by Codex.

Within repository project instructions:

- more specific nested instructions take precedence for files in their scope;
- this root file defines the default behavior for the repository.

The current user request always determines the task scope. Do not treat this file as permission to implement unrelated roadmap features.
