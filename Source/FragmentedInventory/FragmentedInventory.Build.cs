// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FragmentedInventory : ModuleRules
{
	public FragmentedInventory(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"AdvancedAsset",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"NetCore",
				"StructUtils"
			}
		);
	}
}
