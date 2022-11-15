// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../UE4_osgbLoader.h"
#include "GameFramework/Actor.h"
#include "MyRuntimeMeshActor.generated.h"

struct MeshSection;
/**
 * 
 */
UCLASS()
class UE4_OSGBLOADER_API AMyRuntimeMeshActor : public AActor
{
	GENERATED_BODY()
public:
	AMyRuntimeMeshActor();
	UMaterialInterface* _defaultMaterial;
	static UMaterialInterface* GetDefaultMaterial();

private:
	UPROPERTY(EditAnywhere)
		class URuntimeMeshComponentStatic* _runtimeMeshComponentStatic;
//#if USE_RuntimeMeshComponent
//	UPROPERTY(EditAnywhere)
//	class URuntimeMeshComponentStatic* _runtimeMeshComponentStatic;
//#else
//private:
//	class UProceduralMeshComponent* _proceduralMeshComponent;
//#endif

public:
	void SetupMaterialSlot(MeshSection* meshSection);
	void CreateSectionFromComponents(MeshSection* meshSection);
	void RemoveSectionFromComponents(MeshSection* meshSection);
};
