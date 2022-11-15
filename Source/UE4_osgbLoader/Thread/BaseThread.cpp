#include "BaseThread"

BaseThread::BaseThread(FString threadName) :
	_threadName(threadName), _bDead(false), _bActive(false), _bFinish(true),
	_pThread(nullptr), _pStartEvent(nullptr)
{
}

bool BaseThread::Create(FEvent* pStartEvent, uint32 stackSize /*= (32 * 1024)*/, EThreadPriority threadPriority /*= TPri_Normal*/)
{
	_pStartEvent = pStartEvent;
	_pThread = FRunnableThread::Create(this, *_threadName, stackSize, threadPriority);
	if (_pThread)
	{
		UE_LOG(LogTemp, Warning, TEXT("Create %s"), *_threadName);
		return true;
	}
	return false;
}

void BaseThread::StartThread()
{
	if (_pStartEvent)
	{
		check(_pThread);
		_pStartEvent->Trigger();
		_pStartEvent = nullptr;
	}
}

void BaseThread::StopThread(bool bShouldWait /*= true*/)
{
	_bDead.store(true);
	_pThread->Suspend(false);
	_pThread->Kill(bShouldWait);
	_bActive.store(false);
}

void BaseThread::Wakeup()
{
	if (_bDead.load() == true || _bActive.load())
		return;
	UE_LOG(LogTemp, Warning, TEXT("Wakeup Thread: %s"), *_threadName);
	_pThread->Suspend(false);
}

BaseThread::~BaseThread()
{
	delete _pThread;
	_pThread = nullptr;
}