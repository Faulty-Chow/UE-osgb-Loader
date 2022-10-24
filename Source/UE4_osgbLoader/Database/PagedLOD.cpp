#include "PagedLOD"
#include "Geometry"
#include "Model"
#include "NodeVisitor"
#include "../Instance/MyRuntimeMeshActor.h"
#include "../Instance/OsgbLoaderThreadPool"

#include "Kismet/KismetSystemLibrary.h"

#include <osg/Node>
#include <osgDB/ReadFile>

PagedLOD::PagedLOD(Model* owner) :
	_owner(owner), _parent(nullptr), _index(-1), _bActive(false), _bNeedReload(false)
{
	check(IsInGameThread());
	_lastFrameUsed= UKismetSystemLibrary::GetFrameCount();

	std::string filePath = _owner->_folderPath + "\\" + GetName() + ".osgb";
	osg::Node* node = osgDB::readNodeFile(filePath);
	if (node != nullptr)
	{
		PagedLODVisitor plodVisitor(this);
		node->accept(plodVisitor);
		for (auto geometry : _geometries)
		{
			for (auto meshSection : *geometry->_meshSections)
			{
				meshSection->_texture = UTexture2D::CreateTransient(
					meshSection->_cols, meshSection->_rows, PF_B8G8R8A8);
				meshSection->_material = UMaterialInstanceDynamic::Create(
					OsgbLoaderThreadPool::GetInstance()->GetDefaultMaterial(), OsgbLoaderThreadPool::GetInstance()->GetWorld(), NAME_None);
				OsgbLoaderThreadPool::GetInstance()->GetRuntimeMeshActor()->SetupMaterialSlot(meshSection);
				uint8* MipData = static_cast<uint8*>(meshSection->_texture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
				FMemory::Memcpy(MipData, meshSection->_textureData, meshSection->_cols * meshSection->_rows * 4);
				meshSection->_texture->PlatformData->Mips[0].BulkData.Unlock();
				meshSection->_texture->UpdateResource();
				meshSection->_material->SetTextureParameterValue("Param", meshSection->_texture);
				delete meshSection->_textureData;
				meshSection->_textureData = nullptr;
			}
		}
	}
	else
		UE_LOG(LogTemp, Error, TEXT("New PagedLOD faild by readNodeFile()."));
}

PagedLOD::PagedLOD(Geometry* precursor) :
	_owner(precursor->_owner->_owner), _parent(precursor->_owner),
	_index(precursor->_index), _bActive(0), _bNeedReload(false)
{
	_lastFrameUsed = precursor->_fileReadRequest->_frameNumberLastRequest;
}
PagedLOD::~PagedLOD()
{
	for (Geometry* geometry : _geometries)
		delete geometry;
	for (PagedLOD* child : _children)
	{
		if (child != nullptr)
		{
			delete child;
			child = nullptr;
		}
	}
	_children.clear();
	_geometries.clear();
}

void PagedLOD::AddChild(uint32 index, PagedLOD* child)
{
	if (_children[index] == nullptr)
		_children[index] = child;
	else
	{
		check(_children[index]->_bNeedReload);
		for (int i = 0; i < child->_geometries.size(); i++)
		{
			_children[index]->_geometries[i] = child->_geometries[i];
			child->_geometries[i] = nullptr;
		}
	}
}

void PagedLOD::Release()
{
	_bNeedReload = true;
	UE_LOG(LogTemp, Error, TEXT("Release PagedLOD: %s"), *FString(GetName().c_str()));
	for (Geometry* geometry : _geometries)
	{
		for (MeshSection* meshSection : *geometry->_meshSections)
		{
			meshSection->_vertices->Empty();
			meshSection->_triangles->Empty();
			meshSection->_normals->Empty();
			meshSection->_vertexColors->Empty();
			meshSection->_UV->Empty();
			meshSection->_tangent->Empty();
		}
	}
	_parent->_geometries[_index]->_fileReadRequest->_bReload = true;
	_parent->_geometries[_index]->_fileReadRequest->_bAllowLoad = true;
}

void PagedLOD::Reload()
{
	_parent->_geometries[_index]->LoadSuccessor();
}

std::string PagedLOD::GetName()
{
	if (_name.empty())
	{
		if (_parent == nullptr)
		{
			_name = _owner->_folderPath.substr(_owner->_folderPath.find_last_of("\\") + 1);
		}
		else
		{
			std::string sourcePath = _parent->_geometries[_index]->_fileReadRequest->_filePath;
			_name = sourcePath.substr(sourcePath.find_last_of('\\') + 1);
			_name = _name.substr(0, _name.length() - 5);
		}
	}
	return _name;
}

bool PagedLOD::operator <<(const Geometry& rhs) const
{
	for (Geometry* geometry : _geometries)
		if (geometry == &rhs)
			return true;
	bool result = false;
	for (PagedLOD* child : _children)
		if (child != nullptr && *child << rhs)
			return true;
	return false;
}

void PagedLOD::MakeGeometryVisible(int32 index)
{
	for (MeshSection* meshSection : *_geometries[index]->_meshSections)
	{
		OsgbLoaderThreadPool::GetInstance()->GetRuntimeMeshActor()->CreateSectionFromComponents(meshSection);
	}
	_bActive |= (1 << index);
}

void PagedLOD::MakeGeometryHide(int32 index)
{
	for (MeshSection* meshSection : *_geometries[index]->_meshSections)
	{
		OsgbLoaderThreadPool::GetInstance()->GetRuntimeMeshActor()->RemoveSectionFromComponents(meshSection);
	}
	_bActive ^= (1 << index);
}