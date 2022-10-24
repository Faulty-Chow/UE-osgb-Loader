// Copyright Epic Games, Inc. All Rights Reserved.

#include "UE4_osgbLoader.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, UE4_osgbLoader, "UE4_osgbLoader" );

std::mutex GameThreadWaitFor;
std::condition_variable GameThreadCondition;