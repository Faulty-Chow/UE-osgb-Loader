// Fill out your copyright notice in the Description page of Project Settings.

#include "RuntimeMeshSubsystem.h"
#include "OsgbLoaderThreadPool"
#include "MyRuntimeMeshActor.h"
#include "Pawn"

void URuntimeMeshSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	_mRuntimeMeshActor = GetWorld()->SpawnActor<AMyRuntimeMeshActor>();

	_databasePath = "F:\\FaultyChow\\terra_osgbs";
	OsgbLoaderThreadPool::Instance = new OsgbLoaderThreadPool(this);
	check(OsgbLoaderThreadPool::Instance->Create());
}

void URuntimeMeshSubsystem::Deinitialize()
{
	OsgbLoaderThreadPool::Instance->CleanView();
	OsgbLoaderThreadPool::Instance->Destroy();
	delete OsgbLoaderThreadPool::Instance;
}

void URuntimeMeshSubsystem::Tick(float deltaTime)
{
	OsgbLoaderThreadPool::GetInstance()->UpdateView();
	Pawn::GetCurrentPawn()->Update(GetWorld());
	std::unique_lock <std::mutex> lock(::GameThreadWaitFor);
	OsgbLoaderThreadPool::GetInstance()->DrawUpNextFrame();
	::GameThreadCondition.wait(lock);
}