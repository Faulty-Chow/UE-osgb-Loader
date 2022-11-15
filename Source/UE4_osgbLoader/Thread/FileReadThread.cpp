#include "FileReadThread"
#include "../Database/NodeVisitor"
#include "../Database/PagedLOD"
#include "../Database/Geometry"
#include "../ThreadPool/RuntimeOsgbLoaderThreadPool"
#include "../Instance/RuntimeMeshSubsystem.h"

#include <osgDB/ReadFile>
#include <osg/Node>

bool FileReadTask::operator < (const FileReadTask& rhs) const
{
	if (*this == rhs)
		return false;
	else if (this->_frameNumberLastRequest != rhs._frameNumberLastRequest)
		return this->_frameNumberLastRequest < rhs._frameNumberLastRequest;
	else
	{
		int ans = strcmp(this->_filePath.c_str(), rhs._filePath.c_str());
		if (ans == 1)
			return true;
		else
			return false;
	}
}

bool FileReadTask::operator == (const FileReadTask& rhs) const
{
	return this->_filePath == rhs._filePath;
}

void FileReadTask::Execute()
{
	osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(_filePath);
	UE_LOG(LogTemp, Warning, TEXT("ReadFile: %s"), *FString(_filePath.c_str()));
	if (node != nullptr)
	{
		if (_bReload)
		{
			PagedLODVisitor plodVisitor(_plod, true);
			node->accept(plodVisitor);
			_plod->_bNeedReload = false;
		}
		else
		{
			PagedLODVisitor plodVisitor(_plod);
			node->accept(plodVisitor);
			FileReadThread::DoInGameThread(
				[plod = _plod](void)->void
				{
					for (auto geometry : plod->_geometries)
					{
						for (auto meshSection : *geometry->_meshSections)
						{
							meshSection->_texture = UTexture2D::CreateTransient(
								meshSection->_cols, meshSection->_rows, PF_B8G8R8A8);
							meshSection->_material = UMaterialInstanceDynamic::Create(
								URuntimeMeshSubsystem::GetRuntimeMeshSubsystem()->GetDefaultMaterial(), 
								URuntimeMeshSubsystem::GetRuntimeMeshSubsystem()->GetWorld(), NAME_None);
							URuntimeMeshSubsystem::GetRuntimeMeshSubsystem()->SetupMaterialSlot(meshSection);
						}
					}
					URuntimeMeshSubsystem::GetRuntimeMeshSubsystem()->RequestCompilePagedLOD(plod);
				}
			);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("FileRead Error: %s"), *FString(_filePath.c_str()));
	}
}

FileReadThread::FileReadThread(RuntimeOsgbLoaderThreadPool* pThreadPool, FString threadName) :
	TaskThread(pThreadPool, threadName)
{
}

bool FileReadThread::ReturnToPool()
{
	check(_task == nullptr);
	/*FileReadTask* newTask = dynamic_cast<RuntimeOsgbLoaderThreadPool*>(_pThreadPool)->GetFileReadTask();
	if (newTask)
	{
		_task = newTask;
		return false;
	}
	else*/
		return true;
}