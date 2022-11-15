// Fill out your copyright notice in the Description page of Project Settings.


#include "ConvStaticMeshActor.h"
#include "Providers/RuntimeMeshProviderStatic.h"
#include "../Database/Geometry"

AConvStaticMeshActor::AConvStaticMeshActor()
{
	_defaultMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("Material'/Game/NewMaterial.NewMaterial'")));
	StaticProvider = NewObject<URuntimeMeshProviderStatic>(this, TEXT("RuntimeMeshProvider-Static"));
	GetRuntimeMeshComponent()->Initialize(StaticProvider);
}

void AConvStaticMeshActor::SetupMaterialSlot(MeshSection* meshSection)
{

	StaticProvider->SetupMaterialSlot(meshSection->_sectionID, FName(meshSection->_material->GetName()), meshSection->_material);

}

void AConvStaticMeshActor::CreateSectionFromComponents(MeshSection* meshSection)
{
	
	StaticProvider->CreateSectionFromComponents(
		0, meshSection->_sectionID, meshSection->_sectionID,
		*meshSection->_vertices, *meshSection->_triangles, *meshSection->_normals,
		*meshSection->_UV, *meshSection->_vertexColors, *meshSection->_tangent,
		ERuntimeMeshUpdateFrequency::Frequent, false
	);

}

void AConvStaticMeshActor::RemoveSectionFromComponents(MeshSection* meshSection)
{

	StaticProvider->RemoveSection(0, meshSection->_sectionID);

}