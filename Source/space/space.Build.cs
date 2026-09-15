// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class space : ModuleRules
{
	public space(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "Slate", "SlateCore", "RenderCore", "ProceduralMeshComponent", "OnlineSubsystem", "VoiceChat" });

		PrivateDependencyModuleNames.AddRange(new string[] { "ApplicationCore", "EngineSettings", "OnlineSubsystemUtils" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
	}
}
