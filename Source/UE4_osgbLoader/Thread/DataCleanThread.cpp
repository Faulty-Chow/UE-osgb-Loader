#include "DataCleanThread"
#include "../Database/Model"
#include "../Database/Geometry"
#include "../Database/PagedLOD"
#include "../Instance/Pawn"

bool DataCleanThread::SetTask(void* task)
{
	if (!_bFinish.load())
		return false;
	_task = static_cast<Model*>(task);
	_bFinish.exchange(_task != nullptr);
	return _bFinish.load();
}

void DataCleanThread::ExecuteTask()
{
	DFS_CleanLODTree(_task->_root);
}

void DataCleanThread::DFS_CleanLODTree(PagedLOD* plod)
{
	if (plod == nullptr)
		return;

	for (Geometry* geometry : plod->_geometries)
		if (_frameNumber - geometry->_lastFrameNumberActive > _cleanFrameout)
			CleanGeometry(geometry);

	for (PagedLOD* child : plod->_children)
		DFS_CleanLODTree(child);
}