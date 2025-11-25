// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ARProceduralSurfaces : ModuleRules
{
	public ARProceduralSurfaces(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		PrivateDependencyModuleNames.AddRange(new string[] { 
        			"ProceduralMeshComponent",
        			"AugmentedReality"});
	}
}
