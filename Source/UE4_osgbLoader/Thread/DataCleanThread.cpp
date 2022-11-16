#include "DataCleanThread"
#include "../Database/Model"
#include "../Database/Geometry"
#include "../Database/PagedLOD"
#include "../Instance/Pawn"
#include "../ThreadPool/OsgbLoaderThreadPool"

void ModelCleanTask::Execute()
{
	check(_model);
	_frameNumber = Pawn::GetCurrentPawn()->GetFrameNumber();
	DFS_CleanLODTree(_model->_root);
}

void ModelCleanTask::DFS_CleanLODTree(class PagedLOD* plod)
{
	if (plod == nullptr)
		return;

	if (!plod->_bActive && plod->_bNeedReload == false &&
		_frameNumber - plod->_lastFrameUsed > _cleanFrameout)
	{
		plod->Release();
	}

	for (PagedLOD* child : plod->_children)
		DFS_CleanLODTree(child);
}

DataCleanThread::DataCleanThread(OsgbLoaderThreadPool* threadPool, FString threadName) :
	TaskThread(threadPool, threadName)
{
}

bool DataCleanThread::ReturnToPool()
{
	check(_task == nullptr);
	/*ModelCleanTask* newTask = dynamic_cast<OsgbLoaderThreadPool*>(_pThreadPool)->GetCleanModelTask();
	if (newTask)
	{
		_task = newTask;
		return false;
	}
	else*/
		return true;
}