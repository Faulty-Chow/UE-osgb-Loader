// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>

#include "CoreMinimal.h"
#include "../UE4_osgbLoader.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RuntimeMeshSubsystem.generated.h"

extern std::mutex GameThreadWaitFor;
extern std::condition_variable GameThreadResumeCondition;

struct MeshSection;
class FileReadTask;
class PagedLOD;
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

public:
	static URuntimeMeshSubsystem* GetRuntimeMeshSubsystem();

	void AsyncLoadOsgbModels(std::string _rootDir);
	void SyncLoadOsgbModels(std::string _rootDir);
	__forceinline void AddModel(class Model* model) { _models.emplace_back(model); }
	__forceinline int GetNumModels() { return _models.size(); }
	__forceinline std::vector<Model*>& GetModels() { return _models; }

	UMaterialInterface* GetDefaultMaterial();
	void SetupMaterialSlot(MeshSection* meshSection);

	void CreateSection(MeshSection* meshSection);
	void RemoveSection(MeshSection* meshSection);

	void RequestNodeFile(FileReadTask* fileReadTask);
	void RequestCompilePagedLOD(PagedLOD* plod);

public:
	__forceinline std::string& GetDatabasePath() { return _databasePath; }
	__forceinline AMyRuntimeMeshActor* getMyRuntimeMeshActor() { return _mRuntimeMeshActor; }

private:
	std::string _databasePath;
	AMyRuntimeMeshActor* _mRuntimeMeshActor;
	class RuntimeOsgbLoaderThreadPool* _RuntimeOsgbLoaderThreadPool;

	std::vector<Model*> _models;

#ifdef _MSVC_LANG
#if _MSVC_LANG >= 201703L
	static inline URuntimeMeshSubsystem* RuntimeMeshSubsystem = nullptr;
#else
	static URuntimeMeshSubsystem* RuntimeMeshSubsystem;
#endif
#endif
};
