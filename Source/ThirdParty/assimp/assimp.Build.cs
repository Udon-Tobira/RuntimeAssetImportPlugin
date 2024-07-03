// Fill out your copyright notice in the Description page of Project Settings.

using System.IO;
using UnrealBuildTool;

public class assimp : ModuleRules
{
    public assimp(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;
        PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "assimp", "include"));

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // Add the import library
            PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "assimp", "lib", "Release", "assimp-vc143-mt.lib"));

            // Delay-load the DLL, so we can load it from the right place first
            PublicDelayLoadDLLs.Add(Path.Combine(ModuleDirectory, "assimp", "bin", "Release", "assimp-vc143-mt.dll"));

            // Ensure that the DLL is staged along with the executable
            RuntimeDependencies.Add("$(PluginDir)/Binaries/ThirdParty/AssimpLibrary/Win64/assimp-vc143-mt.dll");
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            // Delay-load the DLL, so we can load it from the right place first
            PublicDelayLoadDLLs.Add(Path.Combine(ModuleDirectory, "assimp", "bin", "Release", "libassimp-vc143-mt.dylib"));

            // Ensure that the DLL is staged along with the executable
            RuntimeDependencies.Add("$(PluginDir)/Binaries/ThirdParty/AssimpLibrary/libassimp-vc143-mt.dylib");
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            // Add the import library
            PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "assimp", "bin", "arm64-v8a", "libassimp-vc143-mt.so"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            string LibAssimpSoPath = Path.Combine(ModuleDirectory, "assimp", "bin", "libassimp-vc143-mt.so");

            PublicAdditionalLibraries.Add(LibAssimpSoPath);
            PublicDelayLoadDLLs.Add(LibAssimpSoPath);
            RuntimeDependencies.Add(LibAssimpSoPath);
        }
    }
}
