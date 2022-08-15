// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GBufferProcessPlugin : ModuleRules
{
	public GBufferProcessPlugin(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange(
			new string[] {
			}
			);
				
		PrivateIncludePaths.AddRange(
			new string[] {
				"../../../../../Engine/Source/Runtime/Renderer/Private",
			}
		);
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"RenderCore",
			}
		);
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"MovieSceneCapture",
				"RenderCore",
				"RHI",
				"Slate",
				"SlateCore",
				"Renderer",
				"Projects",
				"RenderCore",
			}
		);

		if (Target.Type == TargetType.Editor)
		{
			PublicDependencyModuleNames.Add("UnrealEd");
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}
