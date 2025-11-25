// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Learning_Metasound : ModuleRules
{
	public Learning_Metasound(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Learning_Metasound",
			"Learning_Metasound/Variant_Horror",
			"Learning_Metasound/Variant_Horror/UI",
			"Learning_Metasound/Variant_Shooter",
			"Learning_Metasound/Variant_Shooter/AI",
			"Learning_Metasound/Variant_Shooter/UI",
			"Learning_Metasound/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
