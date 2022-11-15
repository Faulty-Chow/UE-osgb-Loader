#include "PagedLOD"
#include "Geometry"
#include "Model"
#include "NodeVisitor"
#include "../Instance/RuntimeMeshSubsystem.h"
#include "../Thread/FileReadThread"

#include "Kismet/KismetSystemLibrary.h"

#include <osg/Node>
#include <osgDB/ReadFile>

PagedLOD::PagedLOD(Model* owner) :
	_owner(owner), _parent(nullptr), _index(-1), _bActive(0), _bNeedReload(false)
{
	_lastFrameUsed = UKismetSystemLibrary::GetFrameCount();
}

PagedLOD::PagedLOD(Geometry* precursor) :
	_owner(precursor->_owner->_owner), _parent(precursor->_owner),
	_index(precursor->_index), _bActive(0), _bNeedReload(false)
{
	_lastFrameUsed = UKismetSystemLibrary::GetFrameCount();
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
	if (!_parent)	return;
	_bNeedReload = true;
	// UE_LOG(LogTemp, Error, TEXT("Release PagedLOD: %s"), *FString(GetName().c_str()));
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

const std::string& PagedLOD::GetName()
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

std::string PagedLOD::GetName() const
{
	std::string name;
	if (_name.empty())
	{
		if (_parent == nullptr)
		{
			name = _owner->_folderPath.substr(_owner->_folderPath.find_last_of("\\") + 1);
		}
		else
		{
			std::string sourcePath = _parent->_geometries[_index]->_fileReadRequest->_filePath;
			name = sourcePath.substr(sourcePath.find_last_of('\\') + 1);
			name = _name.substr(0, _name.length() - 5);
		}
	}
	else
		name = _name;
	return name;
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

bool PagedLOD::operator == (const PagedLOD& rhs) const
{
	return this->GetName() == rhs.GetName();
}

void PagedLOD::MakeGeometryVisible(int32 index)
{
	for (MeshSection* meshSection : *_geometries[index]->_meshSections)
	{
		URuntimeMeshSubsystem::GetRuntimeMeshSubsystem()->CreateSection(meshSection);
	}
	_bActive |= (1 << index);
}

void PagedLOD::MakeGeometryHide(int32 index)
{
	for (MeshSection* meshSection : *_geometries[index]->_meshSections)
	{
		URuntimeMeshSubsystem::GetRuntimeMeshSubsystem()->RemoveSection(meshSection);
	}
	_bActive ^= (1 << index);
}