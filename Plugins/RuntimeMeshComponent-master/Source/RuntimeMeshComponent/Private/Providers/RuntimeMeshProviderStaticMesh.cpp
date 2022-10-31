// Copyright 2016-2020 TriAxis Games L.L.C. All Rights Reserved.


#include "Providers/RuntimeMeshProviderStaticMesh.h"
#include "RuntimeMeshStaticMeshConverter.h"
#include "Engine/StaticMesh.h"

URuntimeMeshProviderStaticMesh::URuntimeMeshProviderStaticMesh()
	: StaticMesh(nullptr)
	, MaxLOD(RUNTIMEMESH_MAXLODS)
	, ComplexCollisionLOD(0)
{

}

UStaticMesh* URuntimeMeshProviderStaticMesh::GetStaticMesh() const
{
	return StaticMesh;
}

void URuntimeMeshProviderStaticMesh::SetStaticMesh(UStaticMesh* InStaticMesh)
{
	StaticMesh = InStaticMesh;
	UpdateRenderingFromStaticMesh();
	UpdateCollisionFromStaticMesh();
}

int32 URuntimeMeshProviderStaticMesh::GetMaxLOD() const
{
	return MaxLOD;
}

void URuntimeMeshProviderStaticMesh::SetMaxLOD(int32 InMaxLOD)
{
	MaxLOD = InMaxLOD;
	UpdateRenderingFromStaticMesh();
}

int32 URuntimeMeshProviderStaticMesh::GetComplexCollisionLOD() const
{
	return ComplexCollisionLOD;
}

void URuntimeMeshProviderStaticMesh::SetComplexCollisionLOD(int32 InLOD)
{
	ComplexCollisionLOD = InLOD;
	UpdateCollisionFromStaticMesh();
}

void URuntimeMeshProviderStaticMesh::Initialize()
{
	UpdateRenderingFromStaticMesh();
	UpdateCollisionFromStaticMesh();
}

FBoxSphereBounds URuntimeMeshProviderStaticMesh::GetBounds()
{
	if (StaticMesh)
	{
		return StaticMesh->GetBounds();
	}
	return FBoxSphereBounds(ForceInit);
}

bool URuntimeMeshProviderStaticMesh::GetSectionMeshForLOD(int32 LODIndex, int32 SectionId, FRuntimeMeshRenderableMeshData& MeshData)
{ 
	return URuntimeMeshStaticMeshConverter::CopyStaticMeshSectionToRenderableMeshData(StaticMesh, LODIndex, SectionId, MeshData);
}

FRuntimeMeshCollisionSettings URuntimeMeshProviderStaticMesh::GetCollisionSettings()
{
	FRuntimeMeshCollisionSettings NewSettings;
	NewSettings.bUseAsyncCooking = true;
	NewSettings.bUseComplexAsSimple = false;
	NewSettings.CookingMode = ERuntimeMeshCollisionCookingMode::CookingPerformance;
	
	URuntimeMeshStaticMeshConverter::CopyStaticMeshCollisionToCollisionSettings(StaticMesh, NewSettings);

	return NewSettings;
}

bool URuntimeMeshProviderStaticMesh::HasCollisionMesh()
{
	return ComplexCollisionLOD >= 0;
}

bool URuntimeMeshProviderStaticMesh::GetCollisionMesh(FRuntimeMeshCollisionData& CollisionData)
{
	bool bResult = URuntimeMeshStaticMeshConverter::CopyStaticMeshLODToCollisionData(StaticMesh, ComplexCollisionLOD, CollisionData);

	if (bResult)
	{
		for (auto& Section : CollisionData.CollisionSources)
		{
			Section.SourceProvider = this;
		}
	}
	return bResult;
}


void URuntimeMeshProviderStaticMesh::UpdateCollisionFromStaticMesh()
{
	MarkCollisionDirty();
}

void URuntimeMeshProviderStaticMesh::UpdateRenderingFromStaticMesh()
{
	// Setup the LODs and sections

	// Check valid static mesh
	if (StaticMesh == nullptr || StaticMesh->IsPendingKill())
	{
		ConfigureLODs({ FRuntimeMeshLODProperties() });
		MarkCollisionDirty();
		return;
	}

	// Check mesh data is accessible
#if ENGINE_MAJOR_VERSION == 5
	if (!((GIsEditor || StaticMesh->bAllowCPUAccess) && StaticMesh->GetRenderData() != nullptr))
#else
	if (!((GIsEditor || StaticMesh->bAllowCPUAccess) && StaticMesh->RenderData != nullptr))
#endif
	{
		ConfigureLODs({ FRuntimeMeshLODProperties() });
		MarkCollisionDirty();
		return;
	}

	// Copy materials
#if ENGINE_MAJOR_VERSION == 5
	const auto& MaterialSlots = StaticMesh->GetStaticMaterials();
#else
	const auto& MaterialSlots = StaticMesh->StaticMaterials;
#endif
	for (int32 SlotIndex = 0; SlotIndex < MaterialSlots.Num(); SlotIndex++)
	{
		SetupMaterialSlot(SlotIndex, MaterialSlots[SlotIndex].MaterialSlotName, MaterialSlots[SlotIndex].MaterialInterface);
	}

#if ENGINE_MAJOR_VERSION == 5
	const auto& LODResources = StaticMesh->GetRenderData()->LODResources;
#else
	const auto& LODResources = StaticMesh->RenderData->LODResources;
#endif

	// Setup LODs
	TArray<FRuntimeMeshLODProperties> LODs;
	for (int32 LODIndex = 0; LODIndex < LODResources.Num() && LODIndex <= MaxLOD; LODIndex++)
	{
		FRuntimeMeshLODProperties LODProperties;
#if ENGINE_MAJOR_VERSION == 5
		LODProperties.ScreenSize = StaticMesh->GetRenderData()->ScreenSize[LODIndex].Default;
#else
		LODProperties.ScreenSize = StaticMesh->RenderData->ScreenSize[LODIndex].Default;
#endif

		LODs.Add(LODProperties);
	}
	ConfigureLODs(LODs);


	// Create all sections for all LODs
	for (int32 LODIndex = 0; LODIndex < LODResources.Num() && LODIndex <= MaxLOD; LODIndex++)
	{
		const auto& LOD = LODResources[LODIndex];

		for (int32 SectionId = 0; SectionId < LOD.Sections.Num(); SectionId++)
		{
			const auto& Section = LOD.Sections[SectionId];

			FRuntimeMeshSectionProperties SectionProperties;

			CreateSection(LODIndex, SectionId, SectionProperties);
		}
	}
}
