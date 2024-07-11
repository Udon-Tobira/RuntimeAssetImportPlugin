// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ProceduralMeshComponent.h"

#include "AssetImporter.generated.h"

/**
 * Type representing the result of executing
 * ConcertProceduralMeshComponentFromAssetFile function.
 */
UENUM(BlueprintType)
enum class EConstructProceduralMeshComponentFromAssetFileResult : uint8 {
	/* Success to load */
	Success,

	/* Failed to load scene */
	Failure
};

/**
 * Blueprint Function Library for easy import of assets at runtime.
 */
UCLASS()
class RUNTIMEASSETIMPORT_API UAssetImporter: public UBlueprintFunctionLibrary {
	GENERATED_BODY()

public:
	/**
	 * Construct structured Procedural Mesh Component from the specified asset
	 * file. The file format must be one supported by assimp.
	 * @param   FilePath                    Path to the asset file.
	 * @param   ParentMaterialInterface     The base material interface used to
	 *                                      create materials for the imported
	 *                                      meshes.
	 * @param   Owner                       Owner of the returned procedural mesh
	 *                                      component and its descendant.
	 * @param[out]   ConstructProceduralMeshComponentFromAssetFileResult
	 *                  Result of the execution.
	 * @param   ShouldReplicate             Whether the component should be
	 *                                      replicated or not.
	 * @param   ShouldRegisterComponentToOwner    Whether to register components
	 *                                            to Owner. Must be turned ON to
	 *                                            be reflected in the scene.
	 * @return  This function returns the constructed Procedural Mesh Component.
	 *          It also returns the result of the execution with
	 *          ConstructProceduralMeshComponentFromAssetFileResult.
	 *          If the result is Success, the return value is valid,
	 *          If the result is Failure, the return value is nullptr.
	 */
	UFUNCTION(BlueprintCallable,
	          meta = (ExpandEnumAsExecs =
	                      "ConstructProceduralMeshComponentFromAssetFileResult"))
	static UPARAM(DisplayName = "Root Procedural Mesh Component")
	    UProceduralMeshComponent* ConstructProceduralMeshComponentFromAssetFile(
	        const FString& FilePath, UMaterialInterface* ParentMaterialInterface,
	        AActor* Owner,
	        EConstructProceduralMeshComponentFromAssetFileResult&
	             ConstructProceduralMeshComponentFromAssetFileResult,
	        bool ShouldReplicate                = true,
	        bool ShouldRegisterComponentToOwner = true);
};
