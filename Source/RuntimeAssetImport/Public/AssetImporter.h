// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ProceduralMeshComponent.h"

#include "AssetImporter.generated.h"

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
	 * @param   FilePath                    Path to the asset file
	 * @param   ParentMaterialInterface     The base material interface used to
	 *                                      create materials for the imported
	 *                                      meshes.
	 * @param   Owner                       Owner of the returned procedural mesh
	 *                                      component and its descendant
	 * components
	 */
	UFUNCTION(BlueprintCallable)
	static UProceduralMeshComponent*
	    ConstructProceduralMeshComponentFromAssetFile(
	        const FString& FilePath, UMaterialInterface* ParentMaterialInterface,
	        AActor* Owner);
};
