// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include <string>
#include <mutex>
#include <condition_variable>

#include "CoreMinimal.h"
#include "../UE4_osgbLoader.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RuntimeMeshSubsystem.generated.h"

class AMyRuntimeMeshActor;
/**
 * 
 */
UCLASS()
class URuntimeMeshSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual __forceinline TStatId GetStatId() const override {
		RETURN_QUICK_DECLARE_CYCLE_STAT(URuntimeMeshSubsystem, STATGROUP_Tickables);
	}
	virtual bool IsTickable() const override { return !IsTemplate(); }
	virtual void Tick(float deltaTime) override;

	std::string _databasePath;
	AMyRuntimeMeshActor* _mRuntimeMeshActor;

protected:
	
};
