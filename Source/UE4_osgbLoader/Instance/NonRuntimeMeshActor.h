// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NonRuntimeMeshActor.generated.h"

UCLASS()
class UE4_OSGBLOADER_API ANonRuntimeMeshActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ANonRuntimeMeshActor();

	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	virtual void Tick(float deltaTime) override;

private:
	UPROPERTY(EditAnywhere)
		UMaterialInterface* _defaultMaterial;

	UPROPERTY(EditAnywhere)
		FString _rootDir;
};
