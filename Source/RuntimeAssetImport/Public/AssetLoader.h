// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LoadedMeshData.h"

#include "AssetLoader.generated.h"

/**
 * Type representing the result of executing
 * LoadMeshFromAssetFileResult function.
 */
UENUM(BlueprintType)
enum class ELoadMeshFromAssetFileResult : uint8 {
	/* Success to load */
	Success,

	/* Failed to load scene */
	Failure
};

/**
 * Blueprint Function Library for easy loading of assets at runtime.
 */
UCLASS()
class RUNTIMEASSETIMPORT_API UAssetLoader: public UBlueprintFunctionLibrary {
	GENERATED_BODY()

public:
	/**
	 * Load mesh from the specified asset file. The file format must be one
	 * supported by assimp.
	 * @param        FilePath   Path to the asset file.
	 * @param[out]   LoadMeshFromAssetFileResult Result of the execution.
	 * @return  If the result is Success, the return value is valid,
	 *          If the result is Failure, the return value is empty
	 *          (default-constructed).
	 */
	UFUNCTION(BlueprintCallable,
	          meta = (ExpandEnumAsExecs = "LoadMeshFromAssetFileResult"))
	static UPARAM(DisplayName = "Mesh Data") FLoadedMeshData
	    LoadMeshFromAssetFile(
	        const FString&                FilePath,
	        ELoadMeshFromAssetFileResult& LoadMeshFromAssetFileResult);
};
