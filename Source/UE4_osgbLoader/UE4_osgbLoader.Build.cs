// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;
using UnrealBuildTool;

public class UE4_osgbLoader : ModuleRules
{
	public UE4_osgbLoader(ReadOnlyTargetRules Target) : base(Target)
	{
        bUseRTTI = true;

        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "TraceLog",
            "InputCore",
            "ImageWrapper",
            "ProceduralMeshComponent"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });

        PublicIncludePaths.AddRange(new string[] {
            "C:\\Users\\Administrator\\Documents\\OpenSceneGraph-3.6.5\\include",
            "C:\\Users\\Administrator\\Documents\\OpenSceneGraph-3.6.5\\build\\include",
            "C:\\Users\\Administrator\\Documents\\OpenSceneGraph-3.6.5\\3rdParty\\x64\\include"
        });

        string OSG_lib_Dir = "C:\\Users\\Administrator\\Documents\\OpenSceneGraph-3.6.5\\build\\lib";
        PublicAdditionalLibraries.AddRange(new string[] {
            Path.Combine(OSG_lib_Dir, "osg.lib"),
            Path.Combine(OSG_lib_Dir, "osgDB.lib"),
            Path.Combine(OSG_lib_Dir, "OpenThreads.lib")
        });

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
