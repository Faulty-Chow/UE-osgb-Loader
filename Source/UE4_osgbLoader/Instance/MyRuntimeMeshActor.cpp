// Fill out your copyright notice in the Description page of Project Settings.


#include "MyRuntimeMeshActor.h"

#if USE_RuntimeMeshComponent
#include "RuntimeMeshComponent.h"
#include "RuntimeMeshComponentPlugin.h"
#include "Engine/CollisionProfile.h"
#endif

#include "../Database/Geometry"

AMyRuntimeMeshActor::AMyRuntimeMeshActor()
{
	_defaultMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("Material'/Game/NewMaterial.NewMaterial'")));
#if USE_RuntimeMeshComponent

#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 24
	SetCanBeDamaged(false);
#else
	bCanBeDamaged = false;
#endif

	RuntimeMeshComponent = CreateDefaultSubobject<URuntimeMeshComponent>(TEXT("RuntimeMeshComponent0"));
	RuntimeMeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	RuntimeMeshComponent->Mobility = EComponentMobility::Static;

	RuntimeMeshComponent->SetGenerateOverlapEvents(false);
	RootComponent = RuntimeMeshComponent;

	_staticProvider = NewObject<URuntimeMeshProviderStatic>(this, TEXT("RuntimeMeshProvider-Static"));
	RuntimeMeshComponent->Initialize(_staticProvider);
#else
	_proceduralMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMeshComponent"));
	// _proceduralMeshComponent->InitializeComponent();
#endif
}

#if USE_RuntimeMeshComponent
ERuntimeMeshMobility AMyRuntimeMeshActor::GetRuntimeMeshMobility()
{
	if (RuntimeMeshComponent)
	{
		return RuntimeMeshComponent->GetRuntimeMeshMobility();
	}
	return ERuntimeMeshMobility::Static;
}

void AMyRuntimeMeshActor::SetRuntimeMeshMobility(ERuntimeMeshMobility NewMobility)
{
	if (RuntimeMeshComponent)
	{
		RuntimeMeshComponent->SetRuntimeMeshMobility(NewMobility);
	}
}

void AMyRuntimeMeshActor::SetMobility(EComponentMobility::Type InMobility)
{
	if (RuntimeMeshComponent)
	{
		RuntimeMeshComponent->SetMobility(InMobility);
	}
}

EComponentMobility::Type AMyRuntimeMeshActor::GetMobility()
{
	if (RuntimeMeshComponent)
	{
		return RuntimeMeshComponent->Mobility;
	}
	return EComponentMobility::Static;
}
#endif

void AMyRuntimeMeshActor::SetupMaterialSlot(MeshSection* meshSection)
{
#if USE_RuntimeMeshComponent
	_staticProvider->SetupMaterialSlot(meshSection->_sectionID, NAME_None, meshSection->_material);
#else
	_proceduralMeshComponent->SetMaterial(meshSection->_sectionID, meshSection->_material);
#endif
}

void AMyRuntimeMeshActor::CreateSectionFromComponents(MeshSection* meshSection)
{
#if USE_RuntimeMeshComponent
	_staticProvider->CreateSectionFromComponents(
		0, meshSection->_sectionID, meshSection->_sectionID,
		*meshSection->_vertices, *meshSection->_triangles, *meshSection->_normals,
		*meshSection->_UV, *meshSection->_vertexColors, *meshSection->_tangent,
		ERuntimeMeshUpdateFrequency::Frequent, false
	);
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
	_staticProvider->RemoveSection(0, meshSection->_sectionID);
#else
	_proceduralMeshComponent->ClearMeshSection(meshSection->_sectionID);
#endif
}