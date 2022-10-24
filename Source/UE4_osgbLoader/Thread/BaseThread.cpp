#include "BaseThread"
#include "BaseThreadPool"
#include "BaseAsyncTask"

BaseThread::BaseThread(BaseThreadPool* threadPool, FString threadName) :
	_threadName(threadName), _bDone(false), _bActive(true)
{
	check(threadPool);
	_pThreadPool = threadPool;
	_pTask = nullptr;
}

bool BaseThread::Create(uint32 stackSize, EThreadPriority threadPriority)
{
	_pThread = FRunnableThread::Create(this, *_threadName, stackSize, threadPriority);
	if (_pThread == nullptr)
		return false;
	_threadID = _pThread->GetThreadID();
	return true;
}

BaseThread::~BaseThread()
{
	delete _pThread;
	_pThread = nullptr;
	_pThreadPool = nullptr;
	if (_pTask)
	{
		delete _pTask;
		_pTask = nullptr;
	}
}

bool BaseThread::ExecuteTask(BaseAsyncTask* task)
{
	if (_bActive || _bDone)
		return false;
	_pTask = task;
	FPlatformMisc::MemoryBarrier();
	_pThread->Suspend(false);
	return true;
}

uint32 BaseThread::Run()
{
	while (!_bDone)
	{
		_pTask=_pThreadPool->ReturnToPool(this);
		while (_pTask == nullptr)
		{
			_bActive = false;
			_pThread->Suspend(true);
		}
		FPlatformMisc::MemoryBarrier();
		_pTask->Execute();
		_pTask->SaveResult();
		delete _pTask;
		_pTask = nullptr;
	}
	_bActive = false;
	return 0;
}

void BaseThread::Stop()
{
	_bDone = true;
	_bActive = false;
	_pThread->Suspend(true);
}

void BaseThread::Kill()
{
	Stop();
	_pThread->Kill(true);
}