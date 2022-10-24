#include "DataManageTask"
#include "../../Database/PagedLOD"
#include "../../Database/Geometry"

int32 DataCleanTask::Execute()
{
	return 0;
}

void DataCleanTask::DFS(PagedLOD* plod)
{
	if (plod == nullptr)
		return;

	////Active => Sleep
	//if (plod->_state == PagedLOD::State::Active)
	//{
	//	bool needSleep = true;
	//	for (Geometry* geometry : plod->_geometries)
	//	{
	//		if (geometry->_bVisible)
	//		{
	//			needSleep = false;
	//			break;
	//		}
	//	}
	//	if (needSleep)
	//		plod->_state == PagedLOD::State::Sleep;
	//}

	//switch (plod->_state)
	//{
	//case PagedLOD::State::Suppress:
	//case PagedLOD::State::Active:
	//	plod->_lastFrameActive = _frameNumber;
	//	break;
	//case PagedLOD::State::Sleep:
	//	if (plod->_lastFrameActive - _frameNumber > _frameToClean)
	//		plod->Release();
	//	break;
	//default:
	//	break;
	//}

	//for (PagedLOD* child : plod->_children)
	//	DFS(child);
}

int32 SetTextureAndMaterial::Execute()
{
	/*for (Geometry* geometry : _plod->_geometries)
	{
		for (MeshSection* meshSection : geometry->_meshSections)
		{
			uint8* mipData = static_cast<uint8*>(meshSection->_texture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
			FMemory::Memcpy(mipData, meshSection->_textureData, meshSection->_cols * meshSection->_rows * 4);
			meshSection->_texture->PlatformData->Mips[0].BulkData.Unlock();
			meshSection->_texture->UpdateResource();
			meshSection->_material->SetTextureParameterValue("Param", meshSection->_texture.Get());
			delete meshSection->_textureData;
		}
	}*/
	return 0;
}

void SetTextureAndMaterial::SaveResult()
{
	// _plod->_state = PagedLOD::State::Sleep;
}