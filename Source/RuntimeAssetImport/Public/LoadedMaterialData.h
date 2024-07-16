// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "LoadedMaterialData.generated.h"

/**
 * Data of the loaded material.
 */
USTRUCT(BlueprintType)
struct RUNTIMEASSETIMPORT_API FLoadedMaterialData {
	GENERATED_BODY()

	// Material diffuse color, available only if HasTexture is false.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FLinearColor Color;

	// Texture data compressed into some format, available only if HasTexture is
	// true.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<uint8> CompressedTextureData;

	// Whether there exists texture or not.
	// If there is a texture, the texture data is stored in CompressedTextureData.
	// (Color property is not available);
	// if there is no texture, the Color property is set (CompressedTextureData is
	// not available)
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool HasTexture = false;
};
