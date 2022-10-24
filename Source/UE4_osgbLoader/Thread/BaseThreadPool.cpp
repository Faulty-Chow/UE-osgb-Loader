#include "BaseThreadPool"
#include "BaseThread"
#include "BaseAsyncTask"

BaseThreadPool::~BaseThreadPool()
{
	for (unsigned int i = 0; i < _poolSize; i++)
	{
		_threadPool[i]->Kill();
		delete _threadPool[i];
	}
	_threadPool.Empty();
	_threadQueue.Empty();

	BaseAsyncTask* pTask;
	while (!_taskQueue.IsEmpty())
	{
		_taskQueue.Dequeue(pTask);
		delete pTask;
		_taskQueue.Pop();
	}

	delete _criticalSection;
}

void BaseThreadPool::AddQueuedTask(BaseAsyncTask* task)
{
	check(task != nullptr);

	if (_bDone)
	{
		task->Abandon();
		return;
	}

	BaseThread* pFreeThread = nullptr;
	FScopeLock Lock(_criticalSection);
	_threadQueue.Dequeue(pFreeThread);
	if (_threadQueue.Pop())
	{
		check(pFreeThread->ExecuteTask(task));
	}
	else
		_taskQueue.Enqueue(task);
}

BaseAsyncTask* BaseThreadPool::ReturnToPool(BaseThread* thread)
{
	check(thread != nullptr);
	BaseAsyncTask* newTask = nullptr;
	if (!_bDone)
	{
		FScopeLock Lock(_criticalSection);
		_taskQueue.Dequeue(newTask);
		if (_taskQueue.Pop())
			return newTask;
		else
			_threadQueue.Enqueue(thread);
	}
	return nullptr;
}

bool BaseThreadPool::Create()
{
	Trace::ThreadGroupBegin(*_poolName);

	bool bCreateSuccessful = true;
	check(_criticalSection == nullptr);
	check(_threadPool.Num() == 0);
	_criticalSection = new FCriticalSection();
	for (uint32 i = 0; i < _poolSize; i++)
	{
		BaseThread* newThread = new BaseThread(this, _poolName + "_" + FString::FromInt(i));
		if (newThread->Create())
		{
			_threadPool.Add(newThread);
			_threadQueue.Enqueue(newThread);
		}
		else
		{
			bCreateSuccessful = false;
			delete newThread;
			break;
		}
	}
	if (!bCreateSuccessful)
	{
#ifdef _LOG_BaseThreadPool
#if _LOG_BaseThreadPool
		UE_LOG(LogTemp, Error, TEXT("ThreadPool %s, Create failed."), *_poolName);
#endif
#endif
		Destroy();
	}

	Trace::ThreadGroupEnd();
	return bCreateSuccessful;
}

void BaseThreadPool::Destroy()
{
	_bDone = true;
	for (auto thread : _threadPool)
		thread->Kill();
	
}