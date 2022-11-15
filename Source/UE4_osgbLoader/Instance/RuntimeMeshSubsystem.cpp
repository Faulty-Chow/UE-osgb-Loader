// Fill out your copyright notice in the Description page of Project Settings.

#include "RuntimeMeshSubsystem.h"
// #include "OsgbLoaderThreadPool"
#include "MyRuntimeMeshActor.h"
#include "Pawn"
#include "../ThreadPool/OsgbLoaderThreadPool"
#include "../Thread/FileReadThread"
#include "../Database/Model"
#include "../Database/Geometry"
#include "../Database/PagedLOD"

#include <io.h>
#include <osgDB/ReadFile>

std::mutex GameThreadWaitFor;
std::condition_variable GameThreadResumeCondition;

#ifdef _MSVC_LANG
#if _MSVC_LANG < 201703L
URuntimeMeshSubsystem* URuntimeMeshSubsystem::RuntimeMeshSubsystem = nullptr;
#endif
#endif

void URuntimeMeshSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UTexture2D* temp = UTexture2D::CreateTransient(1, 1, PF_B8G8R8A8);
	uint8* MipData = static_cast<uint8*>(temp->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
	for (int i = 0; i < 4; i++)
	{
		*(MipData) = MAX_uint8;
		MipData++;
	}
	temp->PlatformData->Mips[0].BulkData.Unlock();
	temp->UpdateResource();

	RuntimeMeshSubsystem = this;
	_mRuntimeMeshActor = GetWorld()->SpawnActor<AMyRuntimeMeshActor>();

	_databasePath = "F:\\FaultyChow\\terra_osgbs(2)\\terra_osgbs";

	_OsgbLoaderThreadPool = new OsgbLoaderThreadPool(_databasePath);
	_OsgbLoaderThreadPool->Create();

	SyncLoadOsgbModels(_databasePath);
}

void URuntimeMeshSubsystem::Deinitialize()
{
	_OsgbLoaderThreadPool->Destroy();
	delete _OsgbLoaderThreadPool;

	for (Model* model : _models)
		delete model;
}

void URuntimeMeshSubsystem::Tick(float deltaTime)
{
	Pawn::GetCurrentPawn()->Update(GetWorld());
	_OsgbLoaderThreadPool->Tick()->Wait();
	//std::unique_lock<std::mutex> lock(GameThreadWaitFor);
	//GameThreadResumeCondition.wait(lock);
}

URuntimeMeshSubsystem* URuntimeMeshSubsystem::GetRuntimeMeshSubsystem()
{
	return RuntimeMeshSubsystem;
}

void URuntimeMeshSubsystem::AsyncLoadOsgbModels(std::string _rootDir)
{
	long long handle = 0;
	struct _finddata_t fileinfo;
	std::string temp_str;
	if ((handle = _findfirst(temp_str.assign(_rootDir).append("\\*").c_str(), &fileinfo)) != -1)
	{
		do
		{
			if ((fileinfo.attrib & _A_SUBDIR))
				if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0)
				{
					Model* model = new Model(_rootDir + "\\" + fileinfo.name);
					_models.push_back(model);
				}
		} while (_findnext(handle, &fileinfo) == 0);
	}
	_findclose(handle);
}

void URuntimeMeshSubsystem::SyncLoadOsgbModels(std::string _rootDir)
{
	long long handle = 0;
	struct _finddata_t fileinfo;
	std::string temp_str;
	if ((handle = _findfirst(temp_str.assign(_rootDir).append("\\*").c_str(), &fileinfo)) != -1)
	{
		do
		{
			if ((fileinfo.attrib & _A_SUBDIR))
				if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0)
				{
					std::string osgbFilePath = _rootDir + "\\" + fileinfo.name + "\\" + fileinfo.name + ".osgb";
					osg::Node* node = osgDB::readNodeFile(osgbFilePath);
					Model* model = new Model(_rootDir + "\\" + fileinfo.name, node);
					_models.push_back(model);
				}
		} while (_findnext(handle, &fileinfo) == 0);
	}
	_findclose(handle);
}

UMaterialInterface* URuntimeMeshSubsystem::GetDefaultMaterial()
{
	return _mRuntimeMeshActor->_defaultMaterial;
}

void URuntimeMeshSubsystem::SetupMaterialSlot(MeshSection* meshSection)
{
	_mRuntimeMeshActor->SetupMaterialSlot(meshSection);
	// _ConvStaticMeshActor->SetupMaterialSlot(meshSection);
}

void URuntimeMeshSubsystem::CreateSection(MeshSection* meshSection)
{
	_mRuntimeMeshActor->CreateSectionFromComponents(meshSection);
	// _ConvStaticMeshActor->CreateSectionFromComponents(meshSection);
}

void URuntimeMeshSubsystem::RemoveSection(MeshSection* meshSection)
{
	_mRuntimeMeshActor->RemoveSectionFromComponents(meshSection);
	//_ConvStaticMeshActor->RemoveSectionFromComponents(meshSection);
}

void URuntimeMeshSubsystem::RequestNodeFile(FileReadTask* fileReadTask)
{
	_OsgbLoaderThreadPool->RequestNodeFile(fileReadTask);
	// OsgbLoaderThreadPool::GetInstance()->ReadNodeFile(fileReadTask);
}

void URuntimeMeshSubsystem::RequestCompilePagedLOD(PagedLOD* plod)
{
	_OsgbLoaderThreadPool->RequestCompilePagedLOD(plod);
	// OsgbLoaderThreadPool::GetInstance()->FinishStructureAndMountToModel(plod);
}