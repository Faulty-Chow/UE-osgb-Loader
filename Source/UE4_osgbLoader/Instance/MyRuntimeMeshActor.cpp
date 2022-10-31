// Fill out your copyright notice in the Description page of Project Settings.


#include "MyRuntimeMeshActor.h"

#if USE_RuntimeMeshComponent
#include "Components/RuntimeMeshComponentStatic.h"
#else
#include "ProceduralMeshComponent.h"
#endif

#include "../Database/Geometry"

AMyRuntimeMeshActor::AMyRuntimeMeshActor()
{
	_defaultMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("Material'/Game/NewMaterial.NewMaterial'")));
#if USE_RuntimeMeshComponent
	_runtimeMeshComponentStatic = CreateDefaultSubobject<URuntimeMeshComponentStatic>(TEXT("RuntimeMeshComponentStatic"));
#else
	_proceduralMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMeshComponent"));
#endif
}

void AMyRuntimeMeshActor::SetupMaterialSlot(MeshSection* meshSection)
{
#if USE_RuntimeMeshComponent
	// _staticProvider->SetupMaterialSlot(meshSection->_sectionID, FName(meshSection->_material->GetName()), meshSection->_material);
	_runtimeMeshComponentStatic->SetupMaterialSlot(meshSection->_sectionID, FName(meshSection->_material->GetName()), meshSection->_material);
#else
	_proceduralMeshComponent->SetMaterial(meshSection->_sectionID, meshSection->_material);
#endif
}

void AMyRuntimeMeshActor::CreateSectionFromComponents(MeshSection* meshSection)
{
#if USE_RuntimeMeshComponent
	_runtimeMeshComponentStatic->CreateSectionFromComponents(
		0, meshSection->_sectionID, meshSection->_sectionID,
		*meshSection->_vertices, *meshSection->_triangles, *meshSection->_normals,
		*meshSection->_UV, *meshSection->_vertexColors, *meshSection->_tangent,
		ERuntimeMeshUpdateFrequency::Frequent, false
	);
	/*_staticProvider->CreateSectionFromComponents(
		0, meshSection->_sectionID, meshSection->_sectionID,
		*meshSection->_vertices, *meshSection->_triangles, *meshSection->_normals,
		*meshSection->_UV, *meshSection->_vertexColors, *meshSection->_tangent,
		ERuntimeMeshUpdateFrequency::Frequent, false
	);*/
#else
	_proceduralMeshComponent->CreateMeshSection(
		meshSection->_sectionID, *meshSection->_vertices, *meshSection->_triangles,
		*meshSection->_normals, *meshSection->_UV, *meshSection->_vertexColors,
		*meshSection->_tangent, false
	);
#endif
}

void AMyRuntimeMeshActor::RemoveSectionFromComponents(MeshSection* meshSection)
{
#if USE_RuntimeMeshComponent
	_runtimeMeshComponentStatic->RemoveSection(0, meshSection->_sectionID);
	//_staticProvider->RemoveSection(0, meshSection->_sectionID);
#else
	_proceduralMeshComponent->ClearMeshSection(meshSection->_sectionID);
#endif
}