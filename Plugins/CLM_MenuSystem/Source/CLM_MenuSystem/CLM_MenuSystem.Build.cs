// Copyright Vedant Vats.

using UnrealBuildTool;

public class CLM_MenuSystem : ModuleRules
{
	public CLM_MenuSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});
	}
}
