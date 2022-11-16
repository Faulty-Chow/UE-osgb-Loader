#include "OsgbLoaderThreadPool"
#include "../Thread/DataCleanThread"
#include "../Thread/LODTreeUpdateThread"
#include "../Thread/ShaderCompileThread"
#include "../Thread/FileReadThread"
#include "../Database/Model"
#include "../Database/PagedLOD"
#include "../Database/Geometry"
#include "../Database/NodeVisitor"
#include "../Instance/Pawn"
#include "../Instance/RuntimeMeshSubsystem.h"
#include "Viewport.h"

#include <osgDB/ReadFile>

#include "Kismet/KismetSystemLibrary.h"

PRAGMA_DISABLE_OPTIMIZATION

#pragma region FileReadTaskQueue

#include <functional>
#include <set>

struct FileReadTaskQueue
{
	static bool CompareFileReadTask(FileReadTask* lhs, FileReadTask* rhs)
	{
		if (lhs->_frameNumberLastRequest == rhs->_frameNumberLastRequest)
			return lhs < rhs;
		else
			return lhs->_frameNumberLastRequest < rhs->_frameNumberLastRequest;
	};

	FileReadTaskQueue(int32 maxSize = 200);

	std::function<bool(FileReadTask*, FileReadTask*)> func_compare;
	int32 _maxSize;
	std::mutex _requestMutex;
	std::set<FileReadTask*, decltype(func_compare)> _requestOnce;
	std::set<FileReadTask*, decltype(func_compare)> _requestRepeatedly;

	void Enqueue(FileReadTask*& request);
	bool Dequeue(FileReadTask*& request);
	void Clear();
	bool Empty();
};

FileReadTaskQueue::FileReadTaskQueue(int32 maxSize /*= 200*/)
{
	func_compare = CompareFileReadTask;
	_maxSize = maxSize;
	_requestOnce = std::set<FileReadTask*, decltype(func_compare)>(func_compare);
	_requestRepeatedly = std::set<FileReadTask*, decltype(func_compare)>(func_compare);
}

void FileReadTaskQueue::Enqueue(FileReadTask*& request)
{
	std::unique_lock<std::mutex> lock(_requestMutex);
	if (request->_bAllowLoad == false) 
		return;

	if (_requestOnce.erase(request) == 1)
	{
		request->_frameNumberLastRequest = Pawn::GetCurrentPawn()->GetFrameNumber();
		_requestRepeatedly.insert(request);
	}
	else
	{
		if (_requestRepeatedly.erase(request) == 1)
		{
			request->_frameNumberLastRequest = Pawn::GetCurrentPawn()->GetFrameNumber();
			_requestRepeatedly.insert(request);
		}
		else
		{
			request->_frameNumberLastRequest = Pawn::GetCurrentPawn()->GetFrameNumber();
			_requestOnce.insert(request);
		}
	}

	if (_requestOnce.size() + _requestRepeatedly.size() > _maxSize)
	{
		if (_requestOnce.size() >= _requestRepeatedly.size())
			_requestOnce.erase(--_requestOnce.end());
		else
			_requestRepeatedly.erase(--_requestRepeatedly.end());
	}
}

bool FileReadTaskQueue::Dequeue(FileReadTask*& request)
{
	std::unique_lock<std::mutex> lock(_requestMutex);

	if (_requestOnce.empty() && _requestRepeatedly.empty())
		return false;

	if (_requestOnce.empty())
	{
		request = *_requestRepeatedly.begin();
		_requestRepeatedly.erase(*_requestRepeatedly.begin());
		request->_bAllowLoad = false;
		return true;
	}
	if (_requestRepeatedly.empty())
	{
		request = *_requestOnce.begin();
		_requestOnce.erase(_requestOnce.begin());
		request->_bAllowLoad = false;
		return true;
	}

	if (*_requestOnce.begin() < *_requestRepeatedly.begin())
	{
		request = *_requestRepeatedly.begin();
		_requestRepeatedly.erase(_requestRepeatedly.begin());
		request->_bAllowLoad = false;
	}
	else
	{
		request = *_requestOnce.begin();
		_requestOnce.erase(_requestOnce.begin());
		request->_bAllowLoad = false;
	}
	return true;
}

void FileReadTaskQueue::Clear()
{
	std::unique_lock<std::mutex> lock(_requestMutex);
	_requestOnce.clear();
	_requestRepeatedly.clear();
}

bool FileReadTaskQueue::Empty()
{
	std::unique_lock<std::mutex> lock(_requestMutex);
	return _requestOnce.empty() && _requestRepeatedly.empty();
}

#pragma endregion

#pragma region ThreadPoolManager

class ThreadPoolManager
{
public:
	ThreadPoolManager(OsgbLoaderThreadPool* pThreadPool) :
		_pThreadPool(pThreadPool) {}

	std::mutex _managerMutex;
	OsgbLoaderThreadPool* _pThreadPool;

	void RequestPrepareNextFrame();
	void RequestCleanModel(Model* model);
	void RequestNodeFile(FileReadTask* task);
	void RequestCompilePagedLOD(PagedLOD* plod);

	bool ReturnToPoolOrGetNewTask(LODTreeUpdateThread* pThread);
	bool ReturnToPoolOrGetNewTask(DataCleanThread* pThread);
	bool ReturnToPoolOrGetNewTask(FileReadThread* pThread);
	bool ReturnToPoolOrGetNewTask(ShaderCompileThread* pThread);

	std::vector<LODTreeUpdateThread*> _freeLODTreeUpdateThreads;
	std::vector<DataCleanThread*> _freeDataCleanThreads;
	std::vector<FileReadThread*> _freeFileReadThreads;
	std::vector<ShaderCompileThread*> _freeShaderCompileThreads;
};

void ThreadPoolManager::RequestPrepareNextFrame()
{
	_pThreadPool->_updateTaskIndex = 0;
	std::unique_lock<std::mutex> lock(_managerMutex);
	for (auto itr = _freeLODTreeUpdateThreads.begin(); itr != _freeLODTreeUpdateThreads.end(); itr++)
	{
		// itr = _freeLODTreeUpdateThreads.erase(itr);
		(*itr)->Wakeup();
	}
}
bool ThreadPoolManager::ReturnToPoolOrGetNewTask(LODTreeUpdateThread* pThread)
{
	{
		std::unique_lock<std::mutex> lock_2(_managerMutex);
		for (auto itr = _freeLODTreeUpdateThreads.begin(); itr != _freeLODTreeUpdateThreads.end(); ++itr)
			if (*itr == pThread)
			{
				_freeLODTreeUpdateThreads.erase(itr);
				break;
			}
	}
	std::unique_lock<std::mutex> lock_1(_pThreadPool->_updateTaskMutex);
	if (_pThreadPool->_updateTaskIndex < 
		URuntimeMeshSubsystem::GetRuntimeMeshSubsystem()->GetModels().size())
	{
		LODTreeUpdateTask* newTask = new LODTreeUpdateTask(
			URuntimeMeshSubsystem::GetRuntimeMeshSubsystem()->GetModels()[_pThreadPool->_updateTaskIndex]);
		++(_pThreadPool->_updateTaskIndex);
		pThread->SetTask(newTask);
		return false;
	}
	else
	{
		lock_1.unlock();
		std::unique_lock<std::mutex> lock_2(_managerMutex);
		_freeLODTreeUpdateThreads.emplace_back(pThread);
		if (_freeLODTreeUpdateThreads.size() == _pThreadPool->_numLODTreeUpdateThread)
			_pThreadPool->ResumeGameThread();
		return true;
	}
}

void ThreadPoolManager::RequestCleanModel(Model* model)
{
	{
		bool bNoNeedForPush = false;
		std::unique_lock<std::mutex> lock(_pThreadPool->_cleanTaskMutex);
		for (auto itr = _pThreadPool->_waitForCleanModels.begin(); itr != _pThreadPool->_waitForCleanModels.end(); itr++)
			if (*itr == model)
			{
				bNoNeedForPush = true;
				break;
			}
		if (!bNoNeedForPush)
			_pThreadPool->_waitForCleanModels.push_back(model);
	}
	{
		std::unique_lock<std::mutex> lock(_managerMutex);
		if (!_freeDataCleanThreads.empty())
		{
			DataCleanThread* pThread = *_freeDataCleanThreads.begin();
			// _freeDataCleanThreads.erase(_freeDataCleanThreads.begin());
			pThread->Wakeup();
		}
	}
	
}
bool ThreadPoolManager::ReturnToPoolOrGetNewTask(DataCleanThread* pThread)
{
	{
		std::unique_lock<std::mutex> lock_2(_managerMutex);
		for (auto itr = _freeDataCleanThreads.begin(); itr != _freeDataCleanThreads.end(); ++itr)
			if (*itr == pThread)
			{
				_freeDataCleanThreads.erase(itr);
				break;
			}
	}
	std::unique_lock<std::mutex> lock_1(_pThreadPool->_cleanTaskMutex);
	if (!_pThreadPool->_waitForCleanModels.empty())
	{
		ModelCleanTask* newTask = new ModelCleanTask(_pThreadPool->_waitForCleanModels.front());
		_pThreadPool->_waitForCleanModels.pop_front();
		pThread->SetTask(newTask);
		return false;
	}
	else
	{
		lock_1.unlock();
		std::unique_lock<std::mutex> lock_2(_managerMutex);
		_freeDataCleanThreads.emplace_back(pThread);
		return true;
	}
}

void ThreadPoolManager::RequestNodeFile(FileReadTask* task)
{
	_pThreadPool->_fileReadRequestQueue->Enqueue(task);
	std::unique_lock<std::mutex> lock(_managerMutex);
	if (!_freeFileReadThreads.empty())
	{
		FileReadThread* pThread = *_freeFileReadThreads.begin();
		// _freeFileReadThreads.erase(_freeFileReadThreads.begin());
		pThread->Wakeup();
	}
}
bool ThreadPoolManager::ReturnToPoolOrGetNewTask(FileReadThread* pThread)
{
	{
		std::unique_lock<std::mutex> lock(_managerMutex);
		for (auto itr = _freeFileReadThreads.begin(); itr != _freeFileReadThreads.end(); ++itr)
			if (*itr == pThread)
			{
				_freeFileReadThreads.erase(itr);
				break;
			}
	}
	FileReadTask* newTask = nullptr;
	if (_pThreadPool->_fileReadRequestQueue->Dequeue(newTask))
	{
		pThread->SetTask(newTask);
		return false;
	}
	else
	{
		std::unique_lock<std::mutex> lock(_managerMutex);
		_freeFileReadThreads.emplace_back(pThread);
		return true;
	}
}

void ThreadPoolManager::RequestCompilePagedLOD(PagedLOD* plod)
{
	check(IsInGameThread());
	// check(_pThreadPool->_numShaderCompileThread == 1);
	_pThreadPool->_waitToCompilePlods.Enqueue(plod);
	if (!_freeShaderCompileThreads.empty())
	{
		ShaderCompileThread* pthread = *_freeShaderCompileThreads.begin();
		// _freeShaderCompileThreads.erase(_freeShaderCompileThreads.begin());
		pthread->Wakeup();
	}
}
bool ThreadPoolManager::ReturnToPoolOrGetNewTask(ShaderCompileThread* pThread)
{
	if (!_freeShaderCompileThreads.empty())
		_freeShaderCompileThreads.erase(_freeShaderCompileThreads.begin());
	PagedLOD* plod;
	if (_pThreadPool->_waitToCompilePlods.Dequeue(plod))
	{
		pThread->SetTask(new ShaderCompileTask(plod));
		return false;
	}
	else
	{
		_freeShaderCompileThreads.emplace_back(pThread);
		return true;
	}
}

#pragma endregion

OsgbLoaderThreadPool::OsgbLoaderThreadPool(std::string rootDir, 
	uint32 numLODTreeUpdateThread, uint32 numDataCleanThread, uint32 numFileReadThread, uint32 numShaderCompileThread) :
	_numLODTreeUpdateThread(numLODTreeUpdateThread),
	_numDataCleanThread(numDataCleanThread),
	_numFileReadThread(numFileReadThread),
	_numShaderCompileThread(numShaderCompileThread)
{
	_rootDir = rootDir;
	_pStartEvent = FPlatformProcess::GetSynchEventFromPool(true);
	_gameThreadPauseEvent = FPlatformProcess::GetSynchEventFromPool(false);
	_fileReadRequestQueue = new FileReadTaskQueue;
	_updateTaskIndex = MAX_int32;
	_pThreadPoolManager = new ThreadPoolManager(this);
}

OsgbLoaderThreadPool::~OsgbLoaderThreadPool()
{
	FPlatformProcess::ReturnSynchEventToPool(_pStartEvent);
	_pStartEvent = nullptr;
	FPlatformProcess::ReturnSynchEventToPool(_gameThreadPauseEvent);
	_gameThreadPauseEvent = nullptr;
	delete _fileReadRequestQueue;
	delete _pThreadPoolManager;
}

bool OsgbLoaderThreadPool::Create()
{
	for (uint32 i = 0; i < _numLODTreeUpdateThread; i++)
	{
		FString threadName = "LODTreeUpdateThread_" + FString::FromInt(i);
		LODTreeUpdateThread* pLODTreeUpdateThread = new LODTreeUpdateThread(this, threadName);
		check(pLODTreeUpdateThread->Create(_pStartEvent));
		_LODTreeUpdateThreadPool.emplace_back(pLODTreeUpdateThread);
	}
	for (uint32 i = 0; i < _numDataCleanThread; i++)
	{
		FString threadName = "DataCleanThread_" + FString::FromInt(i);
		DataCleanThread* pDataCleanThread = new DataCleanThread(this, threadName);
		check(pDataCleanThread->Create(_pStartEvent));
		_DataCleanThreadPool.emplace_back(pDataCleanThread);
	}
	for (uint32 i = 0; i < _numFileReadThread; i++)
	{
		FString threadName = "FileReadThread_" + FString::FromInt(i);
		FileReadThread* pFileReadThread = new FileReadThread(this, threadName);
		check(pFileReadThread->Create(_pStartEvent));
		_FileReadThreadPool.emplace_back(pFileReadThread);
	}
	for (uint32 i = 0; i < _numShaderCompileThread; i++)
	{
		FString threadName = "ShaderCompileThread_" + FString::FromInt(i);
		ShaderCompileThread* pShaderCompileThread = new ShaderCompileThread(this, threadName);
		check(pShaderCompileThread->Create(_pStartEvent));
		_ShaderCompileThreadPool.emplace_back(pShaderCompileThread);
	}
	_pStartEvent->Trigger();
	return true;
}

void OsgbLoaderThreadPool::Destroy()
{
	_pStartEvent->Trigger();
	_gameThreadPauseEvent->Trigger();
	// The threads does not hold any resources, don't need to wait.
	for (uint32 i = 0; i < _numLODTreeUpdateThread; i++)
	{
		_LODTreeUpdateThreadPool[i]->StopThread();
		delete _LODTreeUpdateThreadPool[i];
	}
	_LODTreeUpdateThreadPool.clear();
	for (uint32 i = 0; i < _numDataCleanThread; i++)
	{
		_DataCleanThreadPool[i]->StopThread();
		delete _DataCleanThreadPool[i];
	}
	_DataCleanThreadPool.clear();
	for (uint32 i = 0; i < _numFileReadThread; i++)
	{
		_FileReadThreadPool[i]->StopThread();
		delete _FileReadThreadPool[i];
	}
	_FileReadThreadPool.clear();
	for (uint32 i = 0; i < _numShaderCompileThread; i++)
	{
		_ShaderCompileThreadPool[i]->StopThread();
		delete _ShaderCompileThreadPool[i];
	}
	_ShaderCompileThreadPool.clear();
}

void OsgbLoaderThreadPool::RequestPrepareNextFrame()
{
	_pThreadPoolManager->RequestPrepareNextFrame();
}

void OsgbLoaderThreadPool::RequestCleanModel(Model* model)
{
	_pThreadPoolManager->RequestCleanModel(model);
}

void OsgbLoaderThreadPool::RequestCompilePagedLOD(PagedLOD* plod)
{
	_pThreadPoolManager->RequestCompilePagedLOD(plod);
}

void OsgbLoaderThreadPool::RequestNodeFile(FileReadTask* fileReadTask)
{
	_pThreadPoolManager->RequestNodeFile(fileReadTask);
}

FEvent* OsgbLoaderThreadPool::Tick()
{
	check(IsInGameThread());
	for (Model* model : URuntimeMeshSubsystem::GetRuntimeMeshSubsystem()->GetModels())
	{
		if(model->_isVaild)
			model->UpdateGeometries();
	}
	RequestPrepareNextFrame();
	return _gameThreadPauseEvent;
}

void OsgbLoaderThreadPool::ResumeGameThread()
{
	_gameThreadPauseEvent->Trigger();
	/*std::unique_lock<std::mutex> lock(GameThreadWaitFor);
	GameThreadResumeCondition.notify_all();*/
}

bool OsgbLoaderThreadPool::ReturnToPoolOrGetNewTask(BaseThread* pThread)
{
	if (dynamic_cast<LODTreeUpdateThread*>(pThread))
	{
		return _pThreadPoolManager->ReturnToPoolOrGetNewTask(dynamic_cast<LODTreeUpdateThread*>(pThread));
	}
	if (dynamic_cast<DataCleanThread*>(pThread))
	{
		return _pThreadPoolManager->ReturnToPoolOrGetNewTask(dynamic_cast<DataCleanThread*>(pThread));
	}
	if (dynamic_cast<FileReadThread*>(pThread))
	{
		return _pThreadPoolManager->ReturnToPoolOrGetNewTask(dynamic_cast<FileReadThread*>(pThread));
	}
	if (dynamic_cast<ShaderCompileThread*>(pThread))
	{
		return _pThreadPoolManager->ReturnToPoolOrGetNewTask(dynamic_cast<ShaderCompileThread*>(pThread));
	}
	return true;
}

PRAGMA_ENABLE_OPTIMIZATION