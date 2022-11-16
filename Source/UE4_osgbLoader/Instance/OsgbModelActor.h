// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OsgbModelActor.generated.h"

UCLASS()
class UE4_OSGBLOADER_API AOsgbModelActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AOsgbModelActor();

	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	/*int64 GetFrameNumber();
	float GetPixelSizeInViewport(const FVector& boundSphereCenter, const float boundSphereRadius, bool bABS = true) const;
	bool IsInViewport(const FVector& boundSphereCenter, const float boundSphereRadius) const;
	float DistanceToViewOrigin(const FVector& boundSphereCenter) const;

	void AsyncLoadOsgbModels(std::string _rootDir);
	void SyncLoadOsgbModels(std::string _rootDir);

	UMaterialInterface* GetDefaultMaterial();
	void SetupMaterialSlot(class MeshSection* meshSection);
	void CreateSectionFromComponents(MeshSection* meshSection);
	void RemoveSectionFromComponents(MeshSection* meshSection);

	__forceinline std::vector<class Model*>& GetModels();*/

	virtual void Tick(float deltaTime) override;

private:
	UPROPERTY(EditAnywhere)
		UMaterialInterface* _defaultMaterial;

	UPROPERTY(EditAnywhere)
		FString _databasePath;

	class URuntimeMeshComponentStatic* _pRuntimeMeshComponentStatic;
	// std::vector<class Model*> _models;
	class OsgbLoaderThreadPool* _pOsgbLoaderThreadPool;
};
