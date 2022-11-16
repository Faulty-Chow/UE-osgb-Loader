#include "Viewport.h"

#include"EditorViewportClient.h"
#include "Kismet/KismetSystemLibrary.h"

void Viewport::Tick()
{
	static FSceneView* View = nullptr;
	
	_bValid.store(false);

	if (View)
	{
		delete View;
		View = nullptr;
	}

	_frameNumber = UKismetSystemLibrary::GetFrameCount();

	FEditorViewportClient* ActiveViewport = nullptr;
	int ViewportNum = GEditor->GetAllViewportClients().Num();
	for (int32 ViewIndex = 0; ViewIndex < ViewportNum; ++ViewIndex)
	{
		ActiveViewport = GEditor->GetAllViewportClients()[ViewIndex];
		if (ActiveViewport == GEditor->GetActiveViewport()->GetClient())
			break;
		else
			ActiveViewport = nullptr;
	}
	
	if (ActiveViewport)
	{
		FSceneViewFamilyContext ViewFamily(FSceneViewFamilyContext::ConstructionValues(
			ActiveViewport->Viewport, ActiveViewport->GetScene(), ActiveViewport->EngineShowFlags)
			.SetRealtimeUpdate(ActiveViewport->IsRealtime()));
		View = ActiveViewport->CalcSceneView(&ViewFamily);

		_unconstrainedViewRect = View->UnconstrainedViewRect;
		_viewOrigin = View->ViewMatrices.GetViewOrigin();
		_projectionMatrix = View->ViewMatrices.GetProjectionMatrix();
		_viewProjectionMatrix = View->ViewMatrices.GetViewProjectionMatrix();

		_bValid.store(true);
	}
}

float Viewport::GetPixelSizeInViewport(const FVector& boundSphereCenter, const float boundSphereRadius, bool bABS /*= true*/) const
{
	float far, near, left, right, top, bottom;
	near = _projectionMatrix.M[3][2] / (_projectionMatrix.M[2][2] - 1.f);
	far = _projectionMatrix.M[3][2] / (1.f + _projectionMatrix.M[2][2]);
	left = near * (_projectionMatrix.M[2][0] - 1.f) / _projectionMatrix.M[0][0];
	right = near * (1.f + _projectionMatrix.M[2][0]) / _projectionMatrix.M[0][0];
	top = near * (1.f + _projectionMatrix.M[2][1]) / _projectionMatrix.M[1][1];
	bottom = near * (_projectionMatrix.M[2][1] - 1.f) / _projectionMatrix.M[1][1];

	float fovy = (atanf(top / near) - atanf(bottom / near)) / PI * 180.f;
	float aspectRatio = (right - left) / (top - bottom);

	float distance = FVector::Dist(boundSphereCenter, _viewOrigin);

	float angularSize = 2.f * atanf(boundSphereRadius / distance) / PI * 180.f;
	float dpp = std::max(fovy, (float)1.0e-17) / (float)_unconstrainedViewRect.Height();
	float pixelSize = angularSize / dpp;
	if (bABS)
		return fabs(pixelSize);
	else
		return pixelSize;
}

bool Viewport::IsInViewport(const FVector& boundSphereCenter, const float boundSphereRadius) const
{
	float X = boundSphereCenter.X;
	float Y = boundSphereCenter.Y;
	float Z = boundSphereCenter.Z;
	FMatrix viewProjectionMatrix = _viewProjectionMatrix;
	float left = 
		(_viewProjectionMatrix.M[0][3] + _viewProjectionMatrix.M[0][0]) * X + 
		(_viewProjectionMatrix.M[1][0] + _viewProjectionMatrix.M[1][3]) * Y + 
		(_viewProjectionMatrix.M[2][0] + _viewProjectionMatrix.M[2][3]) * Z + 
		(_viewProjectionMatrix.M[3][0] + _viewProjectionMatrix.M[3][3]);
	float right = 
		(_viewProjectionMatrix.M[0][3] - _viewProjectionMatrix.M[0][0]) * X + 
		(_viewProjectionMatrix.M[1][3] - _viewProjectionMatrix.M[1][0]) * Y + 
		(_viewProjectionMatrix.M[2][3] - _viewProjectionMatrix.M[2][0]) * Z + 
		(_viewProjectionMatrix.M[3][3] - _viewProjectionMatrix.M[3][0]);
	float top = 
		(_viewProjectionMatrix.M[0][3] - _viewProjectionMatrix.M[0][1]) * X + 
		(_viewProjectionMatrix.M[1][3] - _viewProjectionMatrix.M[1][1]) * Y + 
		(_viewProjectionMatrix.M[2][3] - _viewProjectionMatrix.M[2][1]) * Z + 
		(_viewProjectionMatrix.M[3][3] - _viewProjectionMatrix.M[3][1]);
	float bottom = 
		(_viewProjectionMatrix.M[0][3] + _viewProjectionMatrix.M[0][1]) * X + 
		(_viewProjectionMatrix.M[1][3] + _viewProjectionMatrix.M[1][1]) * Y + 
		(_viewProjectionMatrix.M[2][3] + _viewProjectionMatrix.M[2][1]) * Z + 
		(_viewProjectionMatrix.M[3][3] + _viewProjectionMatrix.M[3][1]);
	float near = 
		(_viewProjectionMatrix.M[0][3] + _viewProjectionMatrix.M[0][2]) * X + 
		(_viewProjectionMatrix.M[1][3] + _viewProjectionMatrix.M[1][2]) * Y +
		(_viewProjectionMatrix.M[2][3] + _viewProjectionMatrix.M[2][2]) * Z + 
		(_viewProjectionMatrix.M[3][3] + _viewProjectionMatrix.M[3][2]);
	float far = 
		(_viewProjectionMatrix.M[0][3] - _viewProjectionMatrix.M[0][2]) * X +
		(_viewProjectionMatrix.M[1][3] - _viewProjectionMatrix.M[1][2]) * Y +
		(_viewProjectionMatrix.M[2][3] - _viewProjectionMatrix.M[2][2]) * Z +
		(_viewProjectionMatrix.M[3][3] - _viewProjectionMatrix.M[3][2]);
	return left > -boundSphereRadius && right > -boundSphereRadius &&
		top > -boundSphereRadius && bottom > -boundSphereRadius &&
		near > -boundSphereRadius && far > -boundSphereRadius;
}

float Viewport::DistanceToViewOrigin(const FVector& boundSphereCenter) const
{
	return FVector::Distance(boundSphereCenter, _viewOrigin);
}