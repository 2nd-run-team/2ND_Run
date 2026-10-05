// 작성자 : 임진혁 (Prototype01 JSONL 로그·임시 UMG HUD 의존성)
// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SpacePirate : ModuleRules
{
	public SpacePirate(ReadOnlyTargetRules Target) : base(Target)
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
        PrivateDependencyModuleNames.AddRange(new string[] { "NetCore", "Json", "SlateCore", "Niagara" });

		// 전용 HUD BP의 최초 템플릿 생성은 에디터에서만 사용한다.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "UMGEditor" });
		}

		PublicIncludePaths.AddRange(new string[] {
			"SpacePirate",
			"SpacePirate/Variant_Horror",
			"SpacePirate/Variant_Horror/UI",
			"SpacePirate/Variant_Shooter",
			"SpacePirate/Variant_Shooter/AI",
			"SpacePirate/Variant_Shooter/UI",
			"SpacePirate/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
