// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <mutex>
#include <condition_variable>

#define _LOG_ALLOW 1

#if _LOG_ALLOW
#define _LOG_BaseThreadPool 1
#if _LOG_BaseThreadPool
#define _LOG_BaseThread 1
#if _LOG_BaseThread
#define _LOG_BaseAsyncTask 1
#endif
#endif
#endif

#define USE_FrustumCulling 1
#define USE_RuntimeMeshComponent 1
#define ROV 0