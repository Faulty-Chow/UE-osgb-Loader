#pragma once

#include "CoreMinimal.h"
#include "SceneView.h"

#include <atomic>

class DLLEXPORT Viewport
{
public:
	static Viewport* GetActiveViewport() { 
		static Viewport* ActiveViewport = nullptr;
		if (ActiveViewport == nullptr)
			ActiveViewport = new Viewport();
		return ActiveViewport;
	}

	__forceinline int64 GetFrameNumber() { return _frameNumber; }
	float GetPixelSizeInViewport(const FVector& boundSphereCenter, const float boundSphereRadius, bool bABS = true) const;
	bool IsInViewport(const FVector& boundSphereCenter, const float boundSphereRadius) const;
	float DistanceToViewOrigin(const FVector& boundSphereCenter) const;
	void Tick();

private:
	Viewport() :
		_bValid(false), _frameNumber(-1) {}
	~Viewport() = default;
	Viewport(const Viewport&) = delete;

private:
	std::atomic_bool _bValid;
	int64 _frameNumber;
	FIntRect _unconstrainedViewRect;
	FVector _viewOrigin;
	FMatrix _projectionMatrix;
	FMatrix _viewProjectionMatrix;
};