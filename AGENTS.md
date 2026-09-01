# Jump to Space - Codex Instructions


## Project Overview

Project:
Jump to Space

Engine:
Unreal Engine 5.8

Language:
C++ gameplay systems.
Blueprints are allowed for presentation and editor setup.

Genre:
Space exploration / repair / upgrade / travel.


## Core Gameplay Loop

Current vertical slice:

Earth Base
↓
Explore and collect resources
↓
Repair spaceship
↓
Enter spaceship
↓
Launch
↓
Travel to first planet
↓
Explore and upgrade


Do not implement future systems unless requested.


## Build Rules

Codex may compile the Unreal project when implementing C++ changes.

Before compiling:
- Close Unreal Editor if required.
- Use the project's existing Unreal Build configuration.

Do not repeatedly rebuild without fixing errors.

Do not modify unrelated files just to make compilation pass.

If compilation fails:
- Analyze the first meaningful error.
- Fix the root cause.
- Retry.

Avoid Live Coding builds.


## Unreal C++ Rules

Follow UE5 C++ conventions.

Use:

- UCLASS
- USTRUCT
- UENUM
- UINTERFACE
- UPROPERTY
- UFUNCTION


Naming:

A = Actor classes

U = UObject classes

F = Structs

E = Enums


Prefer:

- Composition
- Components
- Subsystems
- Interfaces


Avoid:

- Giant classes
- Putting all gameplay inside Character
- Excessive Tick usage
- Premature abstraction


## Architecture

Keep systems separated.

Preferred systems:

Characters/
Components/
Systems/
World/
UI/
Items/
Ships/
Planets/


Do not create new large frameworks.

Do not introduce:

- Gameplay Ability System
- Mass
- Multiplayer replication

unless explicitly requested.


## Code Changes

Before creating new systems:

Explain briefly:

1. Purpose
2. Responsibility
3. Dependencies


Make minimal changes.

Do not refactor unrelated code.


## Unreal Assets

Do not manually edit:

- uasset
- umap

If editor work is required:

1. Create C++ foundation.
2. Explain remaining editor steps.


## Validation

After implementation report:

1. Changed files
2. What was implemented
3. Unreal Editor steps
4. Possible compile problems


## Current Goal

Create a playable prototype:

Player
↓
Earth base
↓
Repair spaceship
↓
Launch
↓
Reach first planet


Keep implementation simple.
Avoid premature systems.