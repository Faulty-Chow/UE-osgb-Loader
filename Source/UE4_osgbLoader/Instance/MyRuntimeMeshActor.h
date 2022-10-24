// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../UE4_osgbLoader.h"
#include "GameFramework/Actor.h"

#if USE_RuntimeMeshComponent
#include "RuntimeMeshComponent.h"
#include "Providers/RuntimeMeshProviderStatic.h"
#endif
#include "ProceduralMeshComponent.h"

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

#if USE_RuntimeMeshComponent
private:
	class URuntimeMeshComponent* RuntimeMeshComponent;
	class URuntimeMeshProviderStatic* _staticProvider;

public:
	ERuntimeMeshMobility GetRuntimeMeshMobility();
	void SetRuntimeMeshMobility(ERuntimeMeshMobility NewMobility);

public:
	void SetMobility(EComponentMobility::Type InMobility);
	EComponentMobility::Type GetMobility();
	class URuntimeMeshComponent* GetRuntimeMeshComponent() const { return RuntimeMeshComponent; }
#else
private:
	UProceduralMeshComponent* _proceduralMeshComponent;
#endif
public:
	void SetupMaterialSlot(MeshSection* meshSection);
	void CreateSectionFromComponents(MeshSection* meshSection);
	void RemoveSectionFromComponents(MeshSection* meshSection);
};
