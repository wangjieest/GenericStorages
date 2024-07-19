// Copyright GenericStorages, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GenericStorages : ModuleRules
{
	public GenericStorages(ReadOnlyTargetRules Target)
		: base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.AddRange(new string[]{
			ModuleDirectory + "/Public",
			ModuleDirectory + "/Template",
            ModuleDirectory,
			// ... add public include paths required here ...
		});

		PublicDependencyModuleNames.AddRange(new string[]{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]{
			// ... add private dependencies that you statically link with here ...
		});

		DynamicallyLoadedModuleNames.AddRange(new string[]{
			// ... add any modules that your module loads dynamically here ...
		});

		if (Target.Type != TargetRules.TargetType.Server)
		{
			PrivateDependencyModuleNames.AddRange(new string[]{
				"UMG",
			});
		}

		if (Target.Type == TargetRules.TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]{
				"UnrealEd",
				"SlateCore",
				"Slate",
				"InputCore",
				"GraphEditor",
				"EditorStyle",
				"BlueprintGraph",
				"KismetWidgets",
				"ClassViewer",
				"PropertyEditor",
				"Settings",
			});
		}
        BuildVersion Version;
        if (BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version))
        {
            bool bUE_USE_FPROPERTY = (Version.MajorVersion > 4 || (Version.MajorVersion == 4 && Version.MinorVersion >= 25));
            if (bUE_USE_FPROPERTY)
                PublicDefinitions.Add("UE_USE_UPROPERTY=0");
            else
                PublicDefinitions.Add("UE_USE_UPROPERTY=1");
        }
    }
}
