//#include "View"
//#include "../UE4_osgbLoader.h"
//
//#include"EditorViewportClient.h"
//#include "Kismet/KismetSystemLibrary.h"
//
//View* View::_ActiveViewport = nullptr;
//
//void View::Tick()
//{
//	_frameNumber = UKismetSystemLibrary::GetFrameCount();
//
//	FEditorViewportClient* ActiveViewport = nullptr;
//	int ViewportNum = GEditor->GetAllViewportClients().Num();
//	int ActiveViewportIndex = -1;
//	for (int32 ViewIndex = 0; ViewIndex < ViewportNum; ++ViewIndex)
//	{
//		FEditorViewportClient* ViewportClient = GEditor->GetAllViewportClients()[ViewIndex];
//		if (ViewportClient == GEditor->GetActiveViewport()->GetClient())
//			ActiveViewportIndex = ViewIndex;
//	}
//	ActiveViewport = GEditor->GetAllViewportClients()[ActiveViewportIndex];
//
//	FSceneViewFamilyContext ViewFamily(FSceneViewFamilyContext::ConstructionValues(ActiveViewport->Viewport, ActiveViewport->GetScene(), ActiveViewport->EngineShowFlags)
//		.SetRealtimeUpdate(ActiveViewport->IsRealtime()));
//
//	FSceneView* View = ActiveViewport->CalcSceneView(&ViewFamily);
//
//	_viewMatrices = &View->ViewMatrices;
//	_unconstrainedViewRect = &View->UnconstrainedViewRect;
//}
//
//#define M viewProjectionMatrix.M
//#define X bound_center.X
//#define Y bound_center.Y
//#define Z bound_center.Z
//#define R radius
//
//float View::GetBoundPixelSizeOnView(const osg::BoundingSphere& boundingSphere, bool bABS /*= true*/) const
//{
//	FVector bound_center(-boundingSphere.center().x(), boundingSphere.center().y(), boundingSphere.center().z());
//	bound_center *= 100;
//
//	FMatrix viewProjectionMatrix = _viewMatrices->GetProjectionMatrix();
//	float far, near, left, right, top, bottom;
//	near = M[3][2] / (M[2][2] - 1.f);
//	far = M[3][2] / (1.f + M[2][2]);
//	left = near * (M[2][0] - 1.f) / M[0][0];
//	right = near * (1.f + M[2][0]) / M[0][0];
//	top = near * (1.f + M[2][1]) / M[1][1];
//	bottom = near * (M[2][1] - 1.f) / M[1][1];
//
//	float fovy = (atanf(top / near) - atanf(bottom / near)) / osg::PIf * 180.f;
//	float aspectRatio = (right - left) / (top - bottom);
//
//	float radius = boundingSphere.radius() * 100;
//	float distance = FVector::Dist(bound_center, _viewMatrices->GetViewOrigin());
//
//	float angularSize = 2.f * atanf(R / distance) / osg::PIf * 180.f;
//	float dpp = osg::maximum(fovy, (float)1.0e-17) / (float)_unconstrainedViewRect->Height();
//	float pixelSize = angularSize / dpp;
//	if (bABS)
//		return fabs(pixelSize);
//	else
//		return pixelSize;
//}
//
//bool View::IsBoundOnView(const osg::BoundingSphere& boundingSphere) const
//{
//	FVector bound_center(-boundingSphere.center().x(), boundingSphere.center().y(), boundingSphere.center().z());
//	bound_center *= 100;
//	float radius = boundingSphere.radius() * 200;
//
//	FMatrix viewProjectionMatrix = _viewMatrices->GetViewProjectionMatrix() /*_projectionData.ProjectionMatrix*/;
//	float left = (M[0][3] + M[0][0]) * X + (M[1][0] + M[1][3]) * Y + (M[2][0] + M[2][3]) * Z + (M[3][0] + M[3][3]);
//	float right = (M[0][3] - M[0][0]) * X + (M[1][3] - M[1][0]) * Y + (M[2][3] - M[2][0]) * Z + (M[3][3] - M[3][0]);
//	float top = (M[0][3] - M[0][1]) * X + (M[1][3] - M[1][1]) * Y + (M[2][3] - M[2][1]) * Z + (M[3][3] - M[3][1]);
//	float bottom = (M[0][3] + M[0][1]) * X + (M[1][3] + M[1][1]) * Y + (M[2][3] + M[2][1]) * Z + (M[3][3] + M[3][1]);
//	float near = (M[0][3] + M[0][2]) * X + (M[1][3] + M[1][2]) * Y + (M[2][3] + M[2][2]) * Z + (M[3][3] + M[3][2]);
//	float far = (M[0][3] - M[0][2]) * X + (M[1][3] - M[1][2]) * Y + (M[2][3] - M[2][2]) * Z + (M[3][3] - M[3][2]);
//	return left > -R && right > -R && top > -R && bottom > -R && near > -R && far > -R;
//}
//
//float View::DistanceToViewpoint(const osg::BoundingSphere& boundingSphere) const
//{
//	FVector bound_center(-boundingSphere.center().x(), boundingSphere.center().y(), boundingSphere.center().z());
//	bound_center *= 100;
//	return FVector::Distance(bound_center, _viewMatrices->GetViewOrigin());
//}