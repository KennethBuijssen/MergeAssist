// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MergeAssist : ModuleRules
{
	public MergeAssist(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
			    // ... add other public dependencies that you statically link with here ...
			    "Core",
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
			    // ... add private dependencies that you statically link with here ...	
			    "BlueprintGraph",
			    "CoreUObject",
			    "EditorStyle",
			    "Engine",
			    "GraphEditor",
			    "InputCore",
			    "Kismet",
			    "Merge",
			    "Slate",
			    "SlateCore",
			    "SourceControl",
			    "UnrealEd",
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
