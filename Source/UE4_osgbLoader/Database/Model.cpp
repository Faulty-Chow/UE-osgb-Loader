#include "Model"
#include "PagedLOD"
#include "Geometry"
#include "NodeVisitor"
#include "../Thread/FileReadThread"
#include "../Instance/RuntimeMeshSubsystem.h"

#include "Kismet/KismetSystemLibrary.h"

Model::Model(std::string folderPath) :
	_folderPath(folderPath), _isVaild(false), _frameNumberLastClean(-1), _root(nullptr)
{
	std::string successorFilename = _folderPath + "\\" + _folderPath.substr(_folderPath.find_last_of("\\") + 1) + ".osgb";
	PagedLOD* root = new PagedLOD(this);
	_fileReadTask = new FileReadTask(root, successorFilename);
	_fileReadTask->_frameNumberLastRequest = UKismetSystemLibrary::GetFrameCount();
	URuntimeMeshSubsystem::GetRuntimeMeshSubsystem()->RequestNodeFile(_fileReadTask);
}

Model::Model(std::string folderPath, osg::Node* root) :
	_folderPath(folderPath), _frameNumberLastClean(-1)
{
	check(IsInGameThread());
	if (root)
	{
		_root = new PagedLOD(this);
		PagedLODVisitor plodVisitor(_root);
		root->accept(plodVisitor);
		for (auto geometry : _root->_geometries)
		{
			for (auto meshSection : *geometry->_meshSections)
			{
				meshSection->_texture = UTexture2D::CreateTransient(
					meshSection->_cols, meshSection->_rows, PF_B8G8R8A8);
				meshSection->_material = UMaterialInstanceDynamic::Create(
					URuntimeMeshSubsystem::GetRuntimeMeshSubsystem()->GetDefaultMaterial(),
					URuntimeMeshSubsystem::GetRuntimeMeshSubsystem()->GetWorld(), NAME_None);
				uint8* MipData = static_cast<uint8*>(meshSection->_texture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
				FMemory::Memcpy(MipData, meshSection->_textureData, meshSection->_cols * meshSection->_rows * 4);
				meshSection->_texture->PlatformData->Mips[0].BulkData.Unlock();
				meshSection->_texture->UpdateResource();
				meshSection->_material->SetTextureParameterValue("Param", meshSection->_texture);
				URuntimeMeshSubsystem::GetRuntimeMeshSubsystem()->SetupMaterialSlot(meshSection);
				delete meshSection->_textureData;
				meshSection->_textureData = nullptr;
			}
		}
		_isVaild = true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Model is invaild."));
		_root = nullptr;
		_isVaild = false;
	}
}

void Model::UpdateGeometries()
{
	for (Geometry* geometry : _addToViewList)
	{
		geometry->_owner->MakeGeometryVisible(geometry->_index);
		check(_visibleGeometries.insert(geometry).second);
	}
	_addToViewList.clear();
	for (Geometry* geometry : _removeFromViewList)
	{
		geometry->_owner->MakeGeometryHide(geometry->_index);
		check(_visibleGeometries.erase(geometry) == 1);
	}
	_removeFromViewList.clear();
}

Model::~Model()
{
	delete _root;
}