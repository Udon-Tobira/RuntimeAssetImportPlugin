// Fill out your copyright notice in the Description page of Project Settings.

using System.IO;
using UnrealBuildTool;

public class assimp : ModuleRules
{
    public assimp(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        // get assimp directory path
        var assimpDirectoryPath = Path.Combine(ModuleDirectory, "assimp");

        // get assimp include directory path
        var assimpIncludeDirectoryPath = Path.Combine(assimpDirectoryPath, "include");

        // get assimp lib directory path
        var assimpLibDirectoryPath = Path.Combine(assimpDirectoryPath, "lib");

        // get assimp bin directory path
        var assimpBinDirectoryPath = Path.Combine(assimpDirectoryPath, "bin");

        PublicSystemIncludePaths.Add(assimpIncludeDirectoryPath);

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            var assimpDllFilePath = Path.Combine(assimpBinDirectoryPath, "Release", "*.dll");

            // Add the import library
            PublicAdditionalLibraries.Add(Path.Combine(assimpLibDirectoryPath, "Release", "*.lib"));

            // Delay-load the DLL, so we can load it from the right place first
            PublicDelayLoadDLLs.Add(Path.Combine(assimpDllFilePath));

            // Ensure that the DLL is staged along with the executable
            RuntimeDependencies.Add("$(BinaryOutputDir)", assimpDllFilePath);
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            var assimpDylibFilePath = Path.Combine(assimpBinDirectoryPath, "Release", "*.dylib");

            // Delay-load the DLL, so we can load it from the right place first
            PublicDelayLoadDLLs.Add(Path.Combine(assimpDylibFilePath));

            // Ensure that the DLL is staged along with the executable
            RuntimeDependencies.Add("$(BinaryOutputDir)", assimpDylibFilePath);
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            // Add the import library
            PublicAdditionalLibraries.Add(Path.Combine(assimpBinDirectoryPath, "arm64-v8a", "*.so"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            string LibAssimpSoPath = Path.Combine(assimpBinDirectoryPath, "*.so");

            PublicAdditionalLibraries.Add(LibAssimpSoPath);
            PublicDelayLoadDLLs.Add(LibAssimpSoPath);
            RuntimeDependencies.Add(LibAssimpSoPath);
        }
    }
}
