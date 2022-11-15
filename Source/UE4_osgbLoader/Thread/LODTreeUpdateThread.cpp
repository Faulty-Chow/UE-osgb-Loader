#include "LODTreeUpdateThread"
#include "../Instance/Pawn"
#include "../Database/Model"
#include "../Database/PagedLOD"
#include "../Database/Geometry"
#include "../Thread/FileReadThread"
#include "../ThreadPool/OsgbLoaderThreadPool"

void LODTreeUpdateTask::Execute()
{
	check(_model);
	check(Pawn::GetCurrentPawn()->IsVaild());

	check(_model->_addToViewList.empty());
	check(_model->_removeFromViewList.empty());

	Traverse(_model->_root);
	while (!_UnuseableGeometries.empty())
	{
		PagedLOD* predecessor = nullptr;
		FindUseablePredecessor((*_UnuseableGeometries.begin())->_owner, predecessor);
		if (predecessor->_bActive)
		{
			// 使用前驱节点
		UsePredecessor:
			for (auto itr = _renderNextFrame.begin(); itr != _renderNextFrame.end();)
			{
				if (*predecessor << *(*itr))	// 剔除前驱节点下的地标 防止重复渲染
					itr = _renderNextFrame.erase(itr);
				else
					itr++;
			}
			for (auto itr = _UnuseableGeometries.begin(); itr != _UnuseableGeometries.end();)
			{
				if (*predecessor << *(*itr))
					itr = _UnuseableGeometries.erase(itr);
				else
					itr++;
			}
			for (Geometry* geometry : predecessor->_geometries)
				_renderNextFrame.insert(geometry);
		}
		else
		{
			std::vector<Geometry*> successors;
			if (FindUseableSuccessors((*_UnuseableGeometries.begin())->_owner, successors))	// 尝试使用后继节点
			{
				_UnuseableGeometries.erase(_UnuseableGeometries.begin());
				for (Geometry* geometry : successors)
					_renderNextFrame.insert(geometry);
			}
			else		// 依然使用前驱节点
				goto UsePredecessor;
		}
	}

	for (auto itr = _renderNextFrame.begin(); itr != _renderNextFrame.end(); itr++)
		if (_model->_visibleGeometries.count(*itr) == 0)
			_model->_addToViewList.emplace_back(*itr);
	for (auto itr = _model->_visibleGeometries.begin(); itr != _model->_visibleGeometries.end(); itr++)
		if (_renderNextFrame.count(*itr) == 0)
			_model->_removeFromViewList.emplace_back(*itr);
}

bool LODTreeUpdateTask::FindUseableSuccessors(PagedLOD* plod, std::vector<Geometry*>& successors)
{
	if (plod == nullptr) return false;
	if (plod->_bNeedReload)
	{
		for (PagedLOD* child : plod->_children)
			if (FindUseableSuccessors(child, successors) == false)
				return false;
		return true;
	}
	else
	{
		for (Geometry* geometry : plod->_geometries)
			successors.emplace_back(geometry);
		return true;
	}
}

void LODTreeUpdateTask::FindUseablePredecessor(PagedLOD* plod, PagedLOD*& predecessor)
{
	if (plod == nullptr)	return;
	if (plod->_bNeedReload)
		FindUseablePredecessor(plod->_parent, predecessor);
	else
		predecessor = plod;
}

void LODTreeUpdateTask::Traverse(PagedLOD* plod)
{
	if (plod == nullptr)	return;
	for (Geometry* geometry : plod->_geometries)
	{
		float pixelInSize = Pawn::GetCurrentPawn()->GetBoundPixelSizeOnView(geometry->_boundingSphere);
		bool bCulling = false;
		if (USE_FrustumCulling)
			bCulling = !Pawn::GetCurrentPawn()->IsBoundOnView(geometry->_boundingSphere);
		else
			bCulling = false;
		if (!bCulling)
		{
			// 在视锥体内
			if (pixelInSize < geometry->_threshold)	// 满足渲染条件
			{
				plod->_lastFrameUsed = Pawn::GetCurrentPawn()->GetFrameNumber();
				if (plod->_bNeedReload)
				{
					_UnuseableGeometries.emplace_back(geometry);
					plod->Reload();
				}
				else
					_renderNextFrame.insert(geometry);
			}
			else		// 需要LOD层级更高的节点
			{
				geometry->_fileReadRequest->_plod->_lastFrameUsed = Pawn::GetCurrentPawn()->GetFrameNumber();
				if (plod->_children[geometry->_index] == nullptr)	// 还未加载过
				{
					plod->_lastFrameUsed = Pawn::GetCurrentPawn()->GetFrameNumber();
					if (plod->_bNeedReload)
						_UnuseableGeometries.emplace_back(geometry);
					else
						_renderNextFrame.insert(geometry);
					geometry->LoadSuccessor();
				}
				else
					Traverse(plod->_children[geometry->_index]);
			}
		}
		else
		{
			// 预加载视锥体外的		但不添加至渲染队列
			if (pixelInSize < geometry->_threshold)
			{
				plod->_lastFrameUsed = Pawn::GetCurrentPawn()->GetFrameNumber();
				if (plod->_bNeedReload)
					plod->Reload();
			}
			else
			{
				if (plod->_children[geometry->_index] == nullptr)
				{
					plod->_lastFrameUsed = Pawn::GetCurrentPawn()->GetFrameNumber();
					geometry->LoadSuccessor();
				}
				else
					Traverse(plod->_children[geometry->_index]);
			}
		}
	}
}

LODTreeUpdateThread::LODTreeUpdateThread(OsgbLoaderThreadPool* pThreadPool, FString threadName) :
	TaskThread(pThreadPool, threadName)
{
}

PRAGMA_DISABLE_OPTIMIZATION

uint32 LODTreeUpdateThread::Run()
{
	_pStartEvent->Wait();
	UE_LOG(LogTemp, Warning, TEXT("Thread Start: %s"), *_threadName);
	while (!_bDead.load())
	{
		_task = nullptr;
		_bActive.store(!_pThreadPool->ReturnToPoolOrGetNewTask(this));
		if (!_bActive.load())
		{
			UE_LOG(LogTemp, Warning, TEXT("Thread Sleep: %s"), *_threadName);
			_pThread->Suspend(true);
		}
		else
		{
			check(_task);
			_bFinish.store(false);
			_task->Execute();
			dynamic_cast<OsgbLoaderThreadPool*>(_pThreadPool)->
				RequestCleanModel(dynamic_cast<LODTreeUpdateTask*>(_task)->_model);
			_bFinish.store(true);
			delete _task;
			_task = nullptr;
		}
	}
	return 0;
}

bool LODTreeUpdateThread::ReturnToPool()
{
	check(_task == nullptr);
	/*LODTreeUpdateTask* newTask = dynamic_cast<OsgbLoaderThreadPool*>(_pThreadPool)->GetLODTreeUpdateTask();
	if (newTask)
	{
		_task = newTask;
		return false;
	}
	else*/
		return true;
}

PRAGMA_ENABLE_OPTIMIZATION