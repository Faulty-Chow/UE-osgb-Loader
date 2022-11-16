// Fill out your copyright notice in the Description page of Project Settings.


#include "OsgbModelActor.h"
#include "../ThreadPool/OsgbLoaderThreadPool"

#include "Viewport.h"

AOsgbModelActor::AOsgbModelActor()
{
    _defaultMaterial = Cast<UMaterialInterface>(StaticLoadObject(
        UMaterialInterface::StaticClass(), nullptr, TEXT("Material'/Game/NewMaterial.NewMaterial'")));
    _databasePath = "F:\\FaultyChow\\terra_osgbs(2)\\terra_osgbs";
    // _pOsgbLoaderThreadPool = new OsgbLoaderThreadPool(TCHAR_TO_UTF8(*_databasePath));
    // _pOsgbLoaderThreadPool->Create();
}

void AOsgbModelActor::Tick(float deltaTime)
{
    Viewport::GetActiveViewport()->Tick();

}