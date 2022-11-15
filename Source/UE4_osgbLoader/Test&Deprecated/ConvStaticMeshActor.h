// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RuntimeMeshActor.h"
#include "ConvStaticMeshActor.generated.h"

/**
 * 
 */
UCLASS()
class UE4_OSGBLOADER_API AConvStaticMeshActor : public ARuntimeMeshActor
{
	GENERATED_BODY()
public:
	AConvStaticMeshActor();
	UMaterialInterface* _defaultMaterial;
	class URuntimeMeshProviderStatic* StaticProvider;

	void SetupMaterialSlot(struct MeshSection* meshSection);
	void CreateSectionFromComponents(struct MeshSection* meshSection);
	void RemoveSectionFromComponents(struct MeshSection* meshSection);
};
