#include "BaseThread"

BaseThread::BaseThread(FString threadName) :
	_threadName(threadName), _bDead(false), _bActive(false), _bFinish(true),
	_pThread(nullptr), _pStartEvent(nullptr), _pWakeupEvent(nullptr)
{
}

bool BaseThread::Create(FEvent* pStartEvent, uint32 stackSize /*= (32 * 1024)*/, EThreadPriority threadPriority /*= TPri_Normal*/)
{
	_pStartEvent = pStartEvent;
	_pThread = FRunnableThread::Create(this, *_threadName, stackSize, threadPriority);
	if (_pThread)
	{
		_pWakeupEvent = FPlatformProcess::GetSynchEventFromPool(true);
		return true;
	}
	return false;
}

void BaseThread::StartThread()
{
	check(_pThread);
	_bActive.exchange(true);
	_pStartEvent->Trigger();
	_pStartEvent = nullptr;
}

void BaseThread::StopThread(bool bShouldWait /*= true*/)
{
	_bDead.exchange(true);
	Wakeup();
	_pThread->Kill(bShouldWait);
	delete _pThread;
	_pThread = nullptr;
}

void BaseThread::Wakeup()
{
	if (_bDead.load() == true)
		return;
	if (_bActive.load() == false)
	{
		_bActive.exchange(true);
		_pWakeupEvent->Trigger();
	}
}

BaseThread::~BaseThread()
{
	StopThread();

	FPlatformProcess::ReturnSynchEventToPool(_pWakeupEvent);
	_pWakeupEvent = nullptr;
}

uint32 BaseThread::Run()
{
	_pStartEvent->Wait();
	_bActive.exchange(true);
	while (!_bDead.load())
	{

	}
	return 0;
}