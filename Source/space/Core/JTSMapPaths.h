// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Canonical package paths for the expedition flow.
 *
 * Keep these package names in one place so travel, session advertising, and
 * failure recovery cannot silently drift to a legacy map.
 */
namespace JTSMapPaths
{
	inline const TCHAR* const FrontEnd = TEXT("/Game/Space/Maps/L_Entry");
	inline const TCHAR* const PreLaunchLobby = TEXT("/Game/Space/Maps/L_PreLaunchLobby");
	inline const TCHAR* const Earth = TEXT("/Game/Space/Maps/L_EarthLaunchPrototype");
	inline const TCHAR* const SpaceWorld = TEXT("/Game/Space/Maps/L_SpaceWorld");
	inline const TCHAR* const SpaceWorldAsset = TEXT("/Game/Space/Maps/L_SpaceWorld.L_SpaceWorld");
	inline const TCHAR* const LegacyMoonPrototype = TEXT("/Game/Space/Maps/L_MoonPrototype_Tmp");
}
