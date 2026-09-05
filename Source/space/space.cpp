// Copyright Epic Games, Inc. All Rights Reserved.

#include "space.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"

class FSpaceModule final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		const FString ShaderDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders"));
		if (FPaths::DirectoryExists(ShaderDirectory) && !AllShaderSourceDirectoryMappings().Contains(TEXT("/JTS")))
		{
			AddShaderSourceDirectoryMapping(TEXT("/JTS"), ShaderDirectory);
		}
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FSpaceModule, space, "space");
