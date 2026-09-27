// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SecondRun : ModuleRules
{
	public SecondRun(ReadOnlyTargetRules Target) : base(Target)
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

		// 무중력 이동 패킷의 FVector_NetQuantize10 직렬화 구현에 필요하다.
		PrivateDependencyModuleNames.AddRange(new string[] { "NetCore" });

		PublicIncludePaths.AddRange(new string[] {
			"SecondRun",
			"SecondRun/Variant_Horror",
			"SecondRun/Variant_Horror/UI",
			"SecondRun/Variant_Shooter",
			"SecondRun/Variant_Shooter/AI",
			"SecondRun/Variant_Shooter/UI",
			"SecondRun/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
