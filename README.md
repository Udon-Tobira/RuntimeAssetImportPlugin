# RuntimeAssetImportPlugin
## Overview
This is a plugin for the Unreal Engine to load 3D assets (e.g. FBX) at runtime. Made with UE5.4.2.

## Prerequisites
- You are using Windows.
- Download and install [CMake](https://cmake.org/)  
  The check box "Add CMake to the PATH environment variable" that appears during installation must be checked.

## How to install
Clone this repository with its submodules into the Plugins folder (or create your own if you don't have one) in the folder of the Unreal Engine project where you want to install this plugin by doing one of the following:
<details>
<summary>Intallation method 1 (using Github Desktop) (easy)</summary>

1. Launch "Github Desktop" application (if not available, install it first).
2. From the menu, select File > Clone repository...
3. Go to the URL tab.
4. Enter the URL of this Github repository in "URL or username/repository", select the Plugins folder of the project where you want to install this plugin in "Local path", and press Clone.
</details>

<details>
<summary>Intallation method 2 (using git command) (advanced)</summary>

1. If git is not installed, please install it.
2. In the Plugins folder of the project where you want to install this plug-in, start a command prompt.
3. Execute  
   ```
   git clone --recursive URL`
   ```
   Put the URL of this repository in the URL field.
</details>

After installing the plugin using one of the above procedures, open the project and the plugin is enabled.

## Description of the technology inside
We are using assimp as a git submodule, CMake is only needed to build assimp. The actual loading of the asset files is done by assimp, and this plugin only converts them from the format loaded by assimp to a format usable by the Unreal Engine. The build of assimp is done automatically during the project build process. Currently, only Windows is supported.

