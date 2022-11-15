#include "TaskThread"
#include "../ThreadPool/OsgbLoaderThreadPool"

TaskThread::TaskThread(class BaseThreadPool* pThreadPool, FString threadName) :
	BaseThread(threadName), _pThreadPool(pThreadPool)
{
}

bool TaskThread::SetTask(BaseTask* task)
{
	if (!_bFinish.load())
		return false;
	_task = task;
	_bFinish.store(_task == nullptr);
	return !_bFinish.load();
}

uint32 TaskThread::Run()
{
	_pStartEvent->Wait();
	UE_LOG(LogTemp, Warning, TEXT("Thread Start: %s"), *_threadName);
	while (!_bDead.load())
	{
		_task = nullptr;
		_bActive.store(!_pThreadPool->ReturnToPoolOrGetNewTask(this));
		if (!_bActive.load())
		{
			UE_LOG(LogTemp, Warning, TEXT("Sleep Thread: %s"), *_threadName);
			_pThread->Suspend(true);
		}
		else
		{
			check(_task);
			_bFinish.store(false);
			_task->Execute();
			_bFinish.store(true);
		}
	}
	return 0;
}