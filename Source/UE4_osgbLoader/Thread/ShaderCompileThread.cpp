#include "ShaderCompileThread"
#include "HAL/Event.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"

#include "../Database/PagedLOD"
#include "../Database/Geometry"
#include "../Instance/MyRuntimeMeshActor.h"
#include "../Instance/OsgbLoaderThreadPool"

void ShaderCompileThread::Tick()
{
	ExecuteTask();
}

bool ShaderCompileThread::SetTask(void* task)
{
	if (!_bFinish.load())
		return false;
	_task = static_cast<PagedLOD*>(task);
	_bFinish.exchange(_task != nullptr);
	return _bFinish.load();
}

void ShaderCompileThread::ExecuteTask()
{
	check(_task);
	for (auto geometry : _task->_geometries)
	{
		for (auto meshSection : *geometry->_meshSections)
		{
			uint8* MipData = static_cast<uint8*>(meshSection->_texture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
			FMemory::Memcpy(MipData, meshSection->_textureData, meshSection->_cols * meshSection->_rows * 4);
			meshSection->_texture->PlatformData->Mips[0].BulkData.Unlock();
			meshSection->_texture->UpdateResource();
			delete meshSection->_textureData;
			meshSection->_textureData = nullptr;
		}
	}
	DoInGameThread(
		[plod = _task](void) ->void
		{
			for (auto geometry : plod->_geometries)
			{
				for (auto meshSection : *geometry->_meshSections)
				{
					meshSection->_material->SetTextureParameterValue("Param", meshSection->_texture);
					OsgbLoaderThreadPool::GetInstance()->GetRuntimeMeshActor()->SetupMaterialSlot(meshSection);
				}
			}
			check(plod->_parent);
			plod->_parent->AddChild(plod->_index, plod);
		});
	_task = nullptr;
}