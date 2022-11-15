#include "ShaderCompileThread"

#include "../Database/PagedLOD"
#include "../Database/Geometry"
#include "../Database/Model"
#include "../ThreadPool/RuntimeOsgbLoaderThreadPool"

//void ShaderCompileThread::Tick()
//{
//	_bFinish.store(false);
//	if (_task)
//	{
//		_task->Execute();
//		_bFinish.store(true);
//	}
//}
void ShaderCompileTask::Execute()
{
	check(_plod != nullptr);
	//TArray<FString> materialsToCompile;
	for (auto geometry : _plod->_geometries)
	{
		for (auto meshSection : *geometry->_meshSections)
		{
			if (meshSection->_textureData)
			{
				uint8* MipData = static_cast<uint8*>(meshSection->_texture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
				FMemory::Memcpy(MipData, meshSection->_textureData, meshSection->_cols * meshSection->_rows * 4);
				meshSection->_texture->PlatformData->Mips[0].BulkData.Unlock();
#if ENGINE_MAJOR_VERSION == 4
				meshSection->_texture->UpdateResource();
				meshSection->_material->SetTextureParameterValue("Param", meshSection->_texture);
#endif
				// materialsToCompile.Emplace(meshSection->_material->GetName());
				delete meshSection->_textureData;
				meshSection->_textureData = nullptr;
			}
		}
	}
#if ENGINE_MAJOR_VERSION == 5
	ShaderCompileThread::DoInGameThread(
		[plod = _plod](void) ->void
		{
			for (auto geometry : plod->_geometries)
			{
				for (auto meshSection : *geometry->_meshSections)
				{
					meshSection->_texture->UpdateResource();
					meshSection->_material->SetTextureParameterValue("Param", meshSection->_texture);
					// OsgbLoaderThreadPool::GetInstance()->GetRuntimeMeshActor()->SetupMaterialSlot(meshSection);
				}
			}
			if (plod->_parent)
				plod->_parent->AddChild(plod->_index, plod);
			else
			{
				plod->_owner->_root = plod;
				plod->_owner->_isVaild = true;
			}	
		}
	);
#endif
#if ENGINE_MAJOR_VERSION == 4
	if (_plod->_parent)
		_plod->_parent->AddChild(_plod->_index, _plod);
	else
	{
		_plod->_owner->_root = _plod;
		_plod->_owner->_isVaild = true;
	}
#endif
}

ShaderCompileThread::ShaderCompileThread(RuntimeOsgbLoaderThreadPool* pThreadPool, FString threadName) :
	TaskThread(pThreadPool, threadName)/*, FSingleThreadRunnable()*/
{
}

bool ShaderCompileThread::ReturnToPool()
{
	check(_task == nullptr);
	/*ShaderCompileTask* newTask = dynamic_cast<RuntimeOsgbLoaderThreadPool*>(_pThreadPool)->GetShaderCompileTask();
	if (newTask)
	{
		_task = newTask;
		return false;
	}
	else*/
		return true;
}