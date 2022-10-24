#include "Pawn"
#include "Kismet/KismetSystemLibrary.h"

void Pawn::Update(UWorld* _worldContext)
{
	check(IsInGameThread());
	check(_worldContext != nullptr);

	_bVaild.exchange(false);
	_timeStamp = FDateTime::Now();
	_frameNumber = UKismetSystemLibrary::GetFrameCount();

	APlayerController* playerController = _worldContext->GetFirstPlayerController();
	ULocalPlayer* localPlayer = Cast<ULocalPlayer>(playerController->Player);
	check(localPlayer != nullptr);

	FVector out_Location;
	FRotator out_Rotation;
	playerController->GetPlayerViewPoint(out_Location, out_Rotation);
	FMatrix ViewRotationMatrix = FInverseRotationMatrix(out_Rotation) * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));
	if (!ViewRotationMatrix.GetOrigin().IsNearlyZero(0.0f))
	{
		out_Location += ViewRotationMatrix.InverseTransformPosition(FVector::ZeroVector);
		ViewRotationMatrix = ViewRotationMatrix.RemoveTranslation();
	}
	_projectionData.ViewOrigin = out_Location;
	_projectionData.ViewRotationMatrix = ViewRotationMatrix;

	int32 X = FMath::TruncToInt(localPlayer->Origin.X * localPlayer->ViewportClient->Viewport->GetSizeXY().X);
	int32 Y = FMath::TruncToInt(localPlayer->Origin.Y * localPlayer->ViewportClient->Viewport->GetSizeXY().Y);
	uint32 SizeX = FMath::TruncToInt(localPlayer->Size.X * localPlayer->ViewportClient->Viewport->GetSizeXY().X);
	uint32 SizeY = FMath::TruncToInt(localPlayer->Size.Y * localPlayer->ViewportClient->Viewport->GetSizeXY().Y);

	FIntRect UnconstrainedRectangle = FIntRect(X, Y, X + SizeX, Y + SizeY);
	_projectionData.SetViewRectangle(UnconstrainedRectangle);

	FMinimalViewInfo OutViewInfo;
	if (playerController->PlayerCameraManager)
	{
		OutViewInfo = playerController->PlayerCameraManager->CameraCache.POV;
		OutViewInfo.FOV = playerController->PlayerCameraManager->GetFOVAngle();
		playerController->GetPlayerViewPoint(OutViewInfo.Location, OutViewInfo.Rotation);
	}
	else
	{
		playerController->GetPlayerViewPoint(OutViewInfo.Location, OutViewInfo.Rotation);
	}
	FMinimalViewInfo::CalculateProjectionMatrixGivenView(OutViewInfo, localPlayer->AspectRatioAxisConstraint, localPlayer->ViewportClient->Viewport, _projectionData);
	_bVaild.exchange(true);
}

#define M viewProjectionMatrix.M
#define X bound_center.X
#define Y bound_center.Y
#define Z bound_center.Z
#define R radius

float Pawn::GetBoundPixelSizeOnView(const osg::BoundingSphere& boundingSphere, bool bABS) const
{
	FVector bound_center(-boundingSphere.center().x(), boundingSphere.center().y(), boundingSphere.center().z());
	bound_center *= 100;

	FMatrix viewProjectionMatrix = _projectionData.ProjectionMatrix;
	float far, near, left, right, top, bottom;
	near = M[3][2] / (M[2][2] - 1.f);
	far = M[3][2] / (1.f + M[2][2]);
	left = near * (M[2][0] - 1.f) / M[0][0];
	right = near * (1.f + M[2][0]) / M[0][0];
	top = near * (1.f + M[2][1]) / M[1][1];
	bottom = near * (M[2][1] - 1.f) / M[1][1];

	float fovy = (atanf(top / near) - atanf(bottom / near)) / osg::PIf * 180.f;
	float aspectRatio = (right - left) / (top - bottom);

	float radius = boundingSphere.radius() * 100;
	float distance = FVector::Dist(bound_center, _projectionData.ViewOrigin);

	float angularSize = 2.f * atanf(R / distance) / osg::PIf * 180.f;
	float dpp = osg::maximum(fovy, (float)1.0e-17) / (float)_projectionData.GetViewRect().Height();
	float pixelSize = angularSize / dpp;
	if (bABS)
		return fabs(pixelSize);
	else
		return pixelSize;
}
PRAGMA_DISABLE_OPTIMIZATION
bool Pawn::IsBoundOnView(const osg::BoundingSphere& boundingSphere) const
{
	if(ROV && DistanceToViewpoint(boundingSphere) > ROV)
		return false;

	FVector bound_center(-boundingSphere.center().x(), boundingSphere.center().y(), boundingSphere.center().z());
	bound_center *= 100;
	float radius = boundingSphere.radius() * 200;

	FMatrix viewProjectionMatrix = _projectionData.ComputeViewProjectionMatrix() /*_projectionData.ProjectionMatrix*/;
	float left = (M[0][3] + M[0][0]) * X + (M[1][0] + M[1][3]) * Y + (M[2][0] + M[2][3]) * Z + (M[3][0] + M[3][3]);
	float right = (M[0][3] - M[0][0]) * X + (M[1][3] - M[1][0]) * Y + (M[2][3] - M[2][0]) * Z + (M[3][3] - M[3][0]);
	float top = (M[0][3] - M[0][1]) * X + (M[1][3] - M[1][1]) * Y + (M[2][3] - M[2][1]) * Z + (M[3][3] - M[3][1]);
	float bottom = (M[0][3] + M[0][1]) * X + (M[1][3] + M[1][1]) * Y + (M[2][3] + M[2][1]) * Z + (M[3][3] + M[3][1]);
	float near = (M[0][3] + M[0][2]) * X + (M[1][3] + M[1][2]) * Y + (M[2][3] + M[2][2]) * Z + (M[3][3] + M[3][2]);
	float far = (M[0][3] - M[0][2]) * X + (M[1][3] - M[1][2]) * Y + (M[2][3] - M[2][2]) * Z + (M[3][3] - M[3][2]);
	return left > -R && right > -R && top > -R && bottom > -R && near > -R && far > -R;
}
PRAGMA_ENABLE_OPTIMIZATION
float Pawn::DistanceToViewpoint(const osg::BoundingSphere& boundingSphere) const
{
	FVector bound_center(-boundingSphere.center().x(), boundingSphere.center().y(), boundingSphere.center().z());
	bound_center *= 100;
	return FVector::Distance(bound_center, _projectionData.ViewOrigin);
}

