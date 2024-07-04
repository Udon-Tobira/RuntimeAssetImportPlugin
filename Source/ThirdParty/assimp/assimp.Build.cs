// Fill out your copyright notice in the Description page of Project Settings.

using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using UnrealBuildTool;

public class assimp : ModuleRules
{
    public assimp(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        // add assimp cmake build
        var assimpCmakePath = Path.Combine(ModuleDirectory, "assimp");
        ExecuteConsoleCommand(assimpCmakePath, "cmake.exe CMakeLists.txt");
        ExecuteConsoleCommand(assimpCmakePath, "cmake.exe --build . --config Release");

        PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "assimp", "include"));

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // Add the import library
            PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "assimp", "lib", "Release", "assimp-vc143-mt.lib"));

            // Delay-load the DLL, so we can load it from the right place first
            PublicDelayLoadDLLs.Add(Path.Combine(ModuleDirectory, "assimp", "bin", "Release", "assimp-vc143-mt.dll"));

            // Ensure that the DLL is staged along with the executable
            RuntimeDependencies.Add("$(BinaryOutputDir)/assimp-vc143-mt.dll", Path.Combine(PluginDirectory, "Source/ThirdParty/assimp/assimp/bin/Release/assimp-vc143-mt.dll"));
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
    private static void ExecuteConsoleCommand(string workingDirectory, string command)
    {
        var processInfo = new ProcessStartInfo("cmd.exe", "/c " + command)
        {
            CreateNoWindow = true,
            UseShellExecute = false,
            RedirectStandardError = true,
            RedirectStandardOutput = true,
            WorkingDirectory = workingDirectory
        };
        StringBuilder outputString = new StringBuilder();
        Process process = Process.Start(processInfo);

        process.OutputDataReceived += (sender, args) => { outputString.Append(args.Data); Console.WriteLine(args.Data); };
        process.ErrorDataReceived += (sender, args) => { outputString.Append(args.Data); Console.WriteLine(args.Data); };
        process.BeginOutputReadLine();
        process.BeginErrorReadLine();
        process.WaitForExit();

        if (process.ExitCode != 0)
        {
            Console.WriteLine(outputString);
        }
    }
}
