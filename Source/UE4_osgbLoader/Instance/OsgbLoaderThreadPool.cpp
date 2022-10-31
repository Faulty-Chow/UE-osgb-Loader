#include "OsgbLoaderThreadPool"
#include "../Database/Model"
#include "../Database/Geometry"
#include "../Database/PagedLOD"
#include "Pawn"
#include "../Database/NodeVisitor"
#include "RuntimeMeshSubsystem.h"
#include "MyRuntimeMeshActor.h"

#include <string>
#include <io.h>

#include <osg/Node>
#include <osgDB/ReadFile>

#include "Materials/MaterialInterface.h"
#include "ODSC/ODSCManager.h"

#ifdef _MSVC_LANG
#if _MSVC_LANG < 201703L		// C++17
OsgbLoaderThreadPool* OsgbLoaderThreadPool::Instance = nullptr;
#endif
#endif

class BaseThread : public FRunnable
{
public:
	BaseThread() :
		_bDone(true), _bActive(false), _bKill(false) {}
	virtual bool Create(FString threadName, uint32 stackSize = (1024 * 3), EThreadPriority threadPriority = TPri_Normal)
	{
		_pThread = FRunnableThread::Create(this, *threadName, stackSize, threadPriority, FPlatformAffinity::GetPoolThreadMask());
		check(_pThread);
		return true;
	}
	void KillThread(bool bShouldWait = true)
	{
		_pThread->Suspend(true);
		_bKill = true;
		_pThread->Suspend(false);
		if (bShouldWait)
			_pThread->WaitForCompletion();
		_pThread->Kill(bShouldWait);
		_bActive.exchange(false);
	}
	void Pause()
	{
		if (_bActive.load())
		{
			_pThread->Suspend(true);
			_bActive.exchange(false);
		}
	}
	void WakeUp()
	{
		if (!_bActive.load())
		{
			_bActive.exchange(true);
			_pThread->Suspend(false);
		}
	}
	bool DoTask(void* pTask)
	{
		check(_bDone);
		_pTask = pTask;
		WakeUp();
	}

protected:
	virtual uint32 Run() override;
	virtual ~BaseThread()
	{
		delete _pThread;
	}

protected:
	FRunnableThread* _pThread;
	void* _pTask;

	volatile bool _bDone;
	std::atomic_bool _bActive;
	volatile bool _bKill;
};

class UpdateThread : public FRunnable
{
public:
	UpdateThread(FString threadName, uint32 stackSize = (32 * 1024), EThreadPriority threadPriority = TPri_Normal) :
		_bDone(false), _bActive(false)
	{
		_pThread = FRunnableThread::Create(this, *threadName, stackSize, threadPriority);
		check(_pThread);
		_model = nullptr;
	}
	~UpdateThread()
	{
		delete _pThread;
	}
	void Kill(bool bShouldWait = true)
	{
		_pThread->Suspend(true);
		_bDone = true;
		_pThread->Suspend(false);
		_pThread->Kill(bShouldWait);
		_bActive = false;
	}
	void WakeUp()
	{
		if (_bDone || _bActive)
			return;
		_bActive = true;
		//FPlatformMisc::MemoryBarrier();
		_pThread->Suspend(false);
	}
	bool IsActive() { return _bActive; }
	Model* GetTask() { return _model; }
	void SetTask(Model* model)
	{
		check(_model == nullptr);
		_model = model;
	}

private:
	virtual bool Init() override { return FRunnable::Init(); }
	virtual uint32 Run() override
	{
		OsgbLoaderThreadPool::GetInstance()->WaitForStart();
		_bActive = true;
		do
		{
			if (OsgbLoaderThreadPool::GetInstance()->ReturnToPool(this))
			{
				_bActive = false;
				_pThread->Suspend(true);
			}
			else
			{
				check(_model != nullptr);
				Execute();
				OsgbLoaderThreadPool::GetInstance()->CleanModel(_model);
				_model = nullptr;
				_renderNextFrame.clear();
				_UnuseableGeometries.clear();
			}
		} while (!_bDone);
		return 0;
	}
	virtual void Stop() override
	{
		_bDone = true;
		_bActive = false;
		_pThread->Suspend(true);
	}
	virtual void Exit() override { FRunnable::Exit(); }
	void Execute()
	{
		check(Pawn::GetCurrentPawn()->IsVaild());
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
	bool FindUseableSuccessors(PagedLOD* plod, std::vector<Geometry*>& successors)
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
	void FindUseablePredecessor(PagedLOD* plod, PagedLOD*& predecessor)
	{
		if (plod == nullptr)	return;
		if (plod->_bNeedReload)
			FindUseablePredecessor(plod->_parent, predecessor);
		else
			predecessor = plod;
	}
	void Traverse(PagedLOD* plod)
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

private:
	FRunnableThread* _pThread;
	volatile bool _bDone;
	volatile bool _bActive;

	Model* _model;
	std::set<Geometry*> _renderNextFrame;
	std::vector<Geometry*> _UnuseableGeometries;
};

class CleanThread : public FRunnable
{
public:
	// std::mutex _mutex;
public:
	CleanThread(FString threadName, uint32 stackSize = (32 * 1024),
		EThreadPriority threadPriority = TPri_Normal, int64 cleanFrameout = 100) :
		_cleanFrameout(cleanFrameout), _releaseNum(0), _bDone(false), _bActive(false)
	{
		_pThread = FRunnableThread::Create(this, *threadName, stackSize, threadPriority);
		check(_pThread);
		_model = nullptr;
	}
	~CleanThread()
	{
		delete _pThread;
	}
	void Kill(bool bShouldWait = true)
	{
		_pThread->Suspend(true);
		_bDone = true;
		_pThread->Suspend(false);
		_pThread->Kill(bShouldWait);
		_bActive = false;
	}
	/*void Pause()
	{
		if (_bDone || !_bActive.load())
			return;
		_bActive.exchange(false);
		FPlatformMisc::MemoryBarrier();
		_pThread->Suspend(true);
	}*/
	void WakeUp()
	{
		if (_bDone || _bActive.load())
			return;
		// _bActive.exchange(true);
		_pThread->Suspend(false);
		// UE_LOG(LogTemp, Warning, TEXT("Wake up Clean Thread."));
	}
	Model* GetTask() { return _model; }
	void SetTask(Model* newTask)
	{
		check(_model == nullptr);
		_model = newTask;
	}
	bool IsActive() { return _bActive.load(); }

private:
	virtual bool Init() override { return FRunnable::Init(); }
	PRAGMA_DISABLE_OPTIMIZATION
	virtual uint32 Run() override
	{
		OsgbLoaderThreadPool::GetInstance()->WaitForStart();
		// _bActive.exchange(true);
		do
		{
			if (OsgbLoaderThreadPool::GetInstance()->ReturnToPool(this))
			{
				// UE_LOG(LogTemp, Warning, TEXT("Pause Clean thread"));
				// _bActive.exchange(false);
				// FPlatformMisc::MemoryBarrier();
				_pThread->Suspend(true);
				// FPlatformMisc::MemoryBarrier();
			}
			else
			{
				check(_model != nullptr);
				_bActive.exchange(true);
				check(Pawn::GetCurrentPawn()->IsVaild());
				// UE_LOG(LogTemp, Warning, TEXT("Clean model."));
				for (PagedLOD* child : _model->_root->_children)	// 必须保留根节点
					Clean(child);
				_model->_frameNumberLastClean = Pawn::GetCurrentPawn()->GetFrameNumber();
				_releaseNum = 0;
				// std::unique_lock<std::mutex> lock(_mutex);
				_model = nullptr;
				_bActive.exchange(false);
			}
		} while (!_bDone);
		UE_LOG(LogTemp, Error, TEXT("Clean Thread Exit."));
		return 0;
	}
	PRAGMA_ENABLE_OPTIMIZATION
	virtual void Stop() override
	{
		_bDone = true;
		_bActive = (false);
		_pThread->Suspend(true);
	}
	virtual void Exit() override { FRunnable::Exit(); }
	void Clean(PagedLOD* plod)
	{
		if (plod == nullptr)
			return;
		if (!plod->_bActive && plod->_bNeedReload == false &&
			Pawn::GetCurrentPawn()->GetFrameNumber() - plod->_lastFrameUsed > _cleanFrameout)
		{
			_releaseNum++;
			plod->Release();
		}
		for (PagedLOD* child : plod->_children)
			Clean(child);
	}

private:
	FRunnableThread* _pThread;
	volatile bool _bDone;
	std::atomic_bool _bActive;

	Model* _model;
	int64 _cleanFrameout;
	int32 _releaseNum;
};

class ReadThread : public FRunnable
{
	class SpawnTextureAndMaterial
	{
	public:
		SpawnTextureAndMaterial(PagedLOD* plod) :
			_newPagedLOD(plod) {}
		~SpawnTextureAndMaterial() = default;
		FORCEINLINE TStatId GetStatId() const {
			RETURN_QUICK_DECLARE_CYCLE_STAT(SpawnTextureAndMaterial, STATGROUP_TaskGraphTasks);
		}
		static ENamedThreads::Type GetDesiredThread() { return ENamedThreads::GameThread; }
		static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::FireAndForget; }
		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
		{
			check(IsInGameThread());
			for (auto geometry : _newPagedLOD->_geometries)
			{
				for (auto meshSection : *geometry->_meshSections)
				{
					meshSection->_texture = UTexture2D::CreateTransient(
						meshSection->_cols, meshSection->_rows, PF_B8G8R8A8);
					meshSection->_material = UMaterialInstanceDynamic::Create(
						OsgbLoaderThreadPool::GetInstance()->GetDefaultMaterial(), OsgbLoaderThreadPool::GetInstance()->GetWorld(), NAME_None);
					//meshSection->_material->SetTextureParameterValue("Param", meshSection->_texture);
					OsgbLoaderThreadPool::GetInstance()->GetRuntimeMeshActor()->SetupMaterialSlot(meshSection);
				}
			}
			OsgbLoaderThreadPool::GetInstance()->FinishStructureAndMountToModel(_newPagedLOD);
		}
	private:
		PagedLOD* _newPagedLOD;
//#ifdef _MSVC_LANG
//#if _MSVC_LANG >= 201703L		// C++17
//		static __forceinline UMaterialInterface* Material = Cast<UMaterialInterface>(
//			StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("Material'/Game/NewMaterial.NewMaterial'")));
//#else
//		static UMaterialInterface* Material;
//#endif
//#endif
	};

public:
	ReadThread(FString threadName, uint32 stackSize = (32 * 1024),
		EThreadPriority threadPriority = TPri_Normal, int64 cleanFrameout = 300) :
		_bDone(false), _bActive(false)
	{
		_pThread = FRunnableThread::Create(this, *threadName, stackSize, threadPriority);
		check(_pThread);
		_task = nullptr;
	}
	~ReadThread()
	{
		delete _pThread;
	}
	void Kill(bool bShouldWait = true)
	{
		_pThread->Suspend(true);
		_bDone = true;
		_pThread->Suspend(false);
		_pThread->Kill(bShouldWait);
		_bActive = false;
	}
	void WakeUp()
	{
		if (_bDone || _bActive)
			return;
		_bActive = true;
		// FPlatformMisc::MemoryBarrier();
		_pThread->Suspend(false);
	}
	void SetTask(FileReadTask* _newTask)
	{
		check(_task == nullptr);
		_task = _newTask;
	}
	bool IsActive() { return _bActive; }

private:
	virtual bool Init() override { return FRunnable::Init(); }
	virtual uint32 Run() override
	{
		OsgbLoaderThreadPool::GetInstance()->WaitForStart();
		_bActive = true;
		do
		{
			if (OsgbLoaderThreadPool::GetInstance()->ReturnToPool(this))
			{
				_bActive = false;
				_pThread->Suspend(true);
			}
			else
			{
				check(_task != nullptr);
				osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(_task->_filePath);
				// UE_LOG(LogTemp, Warning, TEXT("ReadNodeFile: %s"), *FString(_task->_filePath.c_str()));
				if (node != nullptr)
				{
					if (_task->_bReload)
					{
						PagedLODVisitor plodVisitor(_task->_precursor->_owner->_children[_task->_precursor->_index], true);
						node->accept(plodVisitor);
						_task->_precursor->_owner->_children[_task->_precursor->_index]->_bNeedReload = false;
					}
					else
					{
						PagedLOD* newPlod = new PagedLOD(_task->_precursor);
						PagedLODVisitor plodVisitor(newPlod);
						node->accept(plodVisitor);
						TGraphTask<SpawnTextureAndMaterial>::CreateTask().ConstructAndDispatchWhenReady(newPlod);
					}
				}
				_task = nullptr;
			}
		} while (!_bDone);
		return 0;
	}
	virtual void Stop() override
	{
		_bDone = true;
		_bActive = false;
		_pThread->Suspend(true);
	}
	virtual void Exit() override { FRunnable::Exit(); }

private:
	FRunnableThread* _pThread;
	volatile bool _bDone;
	volatile bool _bActive;

	FileReadTask* _task;
};

class StructureThread : public FRunnable
{
public:
	StructureThread(FString threadName, uint32 stackSize = (32 * 1024), EThreadPriority threadPriority = TPri_Normal) :
		_bDone(false), _bActive(false)
	{
		_pThread = FRunnableThread::Create(this, *threadName, stackSize, threadPriority);
		check(_pThread);
		_plod = nullptr;
	}
	~StructureThread()
	{
		delete _pThread;
	}
	void Kill(bool bShouldWait = true)
	{
		_pThread->Suspend(true);
		_bDone = true;
		_pThread->Suspend(false);
		_pThread->Kill(bShouldWait);
		_bActive = false;
	}
	void WakeUp()
	{
		if (_bDone || _bActive)
			return;
		_bActive = true;
		// FPlatformMisc::MemoryBarrier();
		_pThread->Suspend(false);
	}
	bool IsActive() { return _bActive; }
	void SetTask(PagedLOD* plod)
	{
		check(_plod == nullptr);
		_plod = plod;
	}

private:
	virtual bool Init() override { return FRunnable::Init(); }
	virtual uint32 Run() override
	{
		OsgbLoaderThreadPool::GetInstance()->WaitForStart();
		_bActive = true;
		do
		{
			if (OsgbLoaderThreadPool::GetInstance()->ReturnToPool(this))
			{
				_bActive = false;
				_pThread->Suspend(true);
			}
			else
			{
				check(_plod != nullptr);
				//TArray<FString> materialsToCompile;
				for (auto geometry : _plod->_geometries)
				{
					for (auto meshSection : *geometry->_meshSections)
					{
						uint8* MipData = static_cast<uint8*>(meshSection->_texture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
						FMemory::Memcpy(MipData, meshSection->_textureData, meshSection->_cols * meshSection->_rows * 4);
						meshSection->_texture->PlatformData->Mips[0].BulkData.Unlock();
						meshSection->_texture->UpdateResource();
						meshSection->_material->SetTextureParameterValue("Param", meshSection->_texture);
						//materialsToCompile.Emplace(meshSection->_material->GetName());
						delete meshSection->_textureData;
						meshSection->_textureData = nullptr;
					}
				}
				//FFunctionGraphTask::CreateAndDispatchWhenReady(
				//	[plod = _plod](void) ->void
				//	{
				//		for (auto geometry : plod->_geometries)
				//		{
				//			for (auto meshSection : *geometry->_meshSections)
				//			{
				//				//meshSection->_texture->UpdateResource();
				//				//meshSection->_material->SetTextureParameterValue("Param", meshSection->_texture);
				//				//OsgbLoaderThreadPool::GetInstance()->GetRuntimeMeshActor()->SetupMaterialSlot(meshSection);
				//			}
				//		}
				//		check(plod->_parent);
				//		plod->_parent->AddChild(plod->_index, plod);
				//	},
				//	TStatId(), nullptr, ENamedThreads::GameThread);
				// OsgbLoaderThreadPool::GetInstance()->GetODSCManager()->AddThreadedRequest(materialsToCompile, EShaderPlatform::SP_PCD3D_SM5, true);
				check(_plod->_parent);
				_plod->_parent->AddChild(_plod->_index, _plod);
				_plod = nullptr;
			}
		} while (!_bDone);
		return 0;
	}
	virtual void Stop() override
	{
		_bDone = true;
		_bActive = false;
		_pThread->Suspend(true);
	}
	virtual void Exit() override { FRunnable::Exit(); }

private:
	FRunnableThread* _pThread;
	volatile bool _bDone;
	volatile bool _bActive;

	PagedLOD* _plod;
};

OsgbLoaderThreadPool* OsgbLoaderThreadPool::GetInstance()
{
	return Instance;
}

UWorld* OsgbLoaderThreadPool::GetWorld()
{
	return Instance->_runtimeMeshSubsystem->GetWorld();
}

OsgbLoaderThreadPool::OsgbLoaderThreadPool(URuntimeMeshSubsystem* runtimeMeshSubsystem) :
	_runtimeMeshSubsystem(runtimeMeshSubsystem)
{
	_pODSCManager=new FODSCManager();
}

OsgbLoaderThreadPool::~OsgbLoaderThreadPool()
{
	_fileReadRequestQueue.Clear();
	_waitToMountPlods.Empty();
	_waitForCleanModels.clear();
	for (Model* model : _models)
		delete model;
	_models.clear();
}

void OsgbLoaderThreadPool::LoadModels()
{
	long long handle = 0;
	struct _finddata_t fileinfo;
	std::string temp_str;
	if ((handle = _findfirst(temp_str.assign(_runtimeMeshSubsystem->_databasePath).append("\\*").c_str(), &fileinfo)) != -1)
	{
		do
		{
			if ((fileinfo.attrib & _A_SUBDIR))
				if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0)
				{
					_models.push_back(new Model(_runtimeMeshSubsystem->_databasePath + "\\" + fileinfo.name));
				}
		} while (_findnext(handle, &fileinfo) == 0);
	}
	_findclose(handle);
}

bool OsgbLoaderThreadPool::Create(int32 numUpdateThreads, int32 numReadThreads, int32 numStructureThreads, int32 numCleanThreads)
{
	if (Instance == nullptr)
		return false;
	else
	{
		Instance->LoadModels();
		Instance->_numUpdateThreads = numUpdateThreads;
		Instance->_updateThreads.resize(Instance->_numUpdateThreads);
		Instance->_numReadThreads = numReadThreads;
		Instance->_readThreads.resize(Instance->_numReadThreads);
		Instance->_numStructureThreads = numStructureThreads;
		Instance->_structureThread.resize(Instance->_numStructureThreads);
		Instance->_numCleanThreads = numCleanThreads;
		Instance->_cleanThreads.resize(Instance->_numCleanThreads);

		for (int i = 0; i < Instance->_numUpdateThreads; i++)
		{
			std::string threadName = "UpdateThread_" + std::to_string(i);
			Instance->_updateThreads[i] = new UpdateThread(FString(threadName.c_str()));
		}
		_drawUpComplete = _numUpdateThreads;
		for (int i = 0; i < Instance->_numReadThreads; i++)
		{
			std::string threadName = "ReadThread_" + std::to_string(i);
			Instance->_readThreads[i] = new ReadThread(FString(threadName.c_str()));
		}
		for (int i = 0; i < Instance->_numStructureThreads; i++)
		{
			std::string threadName = "StructureThread" + std::to_string(i);
			Instance->_structureThread[i] = new StructureThread(FString(threadName.c_str()));
		}
		for (int i = 0; i < Instance->_numCleanThreads; i++)
		{
			std::string threadName = "CleanThread" + std::to_string(i);
			Instance->_cleanThreads[i] = new CleanThread(FString(threadName.c_str()));
		}
		std::unique_lock <std::mutex> lock(_initializeMutex);
		_initializeCondition.notify_all();
		return true;
	}
}

void OsgbLoaderThreadPool::Destroy()
{
	for (int i = 0; i < Instance->_numUpdateThreads; i++)
	{
		Instance->_updateThreads[i]->Kill(false);
		delete Instance->_updateThreads[i];
	}
	for (int i = 0; i < Instance->_numReadThreads; i++)
	{
		Instance->_readThreads[i]->Kill(false);
		delete Instance->_readThreads[i];
	}
	for (int i = 0; i < Instance->_numStructureThreads; i++)
	{
		Instance->_structureThread[i]->Kill(false);
		delete Instance->_structureThread[i];
	}
	for (int i = 0; i < Instance->_numReadThreads; i++)
	{
		Instance->_cleanThreads[i]->Kill(false);
		delete Instance->_cleanThreads[i];
	}

	Instance->_numUpdateThreads = 0;
	Instance->_updateThreads.clear();
	Instance->_numReadThreads = 0;
	Instance->_readThreads.clear();
	Instance->_numStructureThreads = 0;
	Instance->_structureThread.clear();
	Instance->_numCleanThreads = 0;
	Instance->_cleanThreads.clear();
}

void OsgbLoaderThreadPool::FileReadTaskQueue::Enqueue(FileReadTask*& request)
{
	std::unique_lock<std::mutex> lock(_requestMutex);
	if (request->_bAllowLoad == false) return;

	if (_requestOnce.erase(request) == 1)
		_requestRepeatedly.insert(request);
	else
	{
		if (_requestRepeatedly.erase(request) == 1)
			_requestRepeatedly.insert(request);
		else
			_requestOnce.insert(request);
	}

	if (_requestOnce.size() + _requestRepeatedly.size() > _maxSize)
	{
		if (_requestOnce.size() >= _requestRepeatedly.size())
			_requestOnce.erase(--_requestOnce.end());
		else
			_requestRepeatedly.erase(--_requestRepeatedly.end());
	}
}

bool OsgbLoaderThreadPool::FileReadTaskQueue::Dequeue(FileReadTask*& request)
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
		request =*_requestOnce.begin();
		_requestOnce.erase(_requestOnce.begin());
		request->_bAllowLoad = false;
	}
	return true;
}

void OsgbLoaderThreadPool::FileReadTaskQueue::Clear()
{
	_requestOnce.clear();
	_requestRepeatedly.clear();
}

void OsgbLoaderThreadPool::UpdateView()
{
	check(IsInGameThread());
	for (Model* model : _models)
		model->UpdateGeometries();
}

void OsgbLoaderThreadPool::DrawUpNextFrame()
{
	_updateModelIndex = 0;
	_drawUpComplete = 0;
	for (UpdateThread* thread : _updateThreads)
		//ReturnToPool(thread);
		thread->WakeUp();
}

bool OsgbLoaderThreadPool::ReturnToPool(UpdateThread* pThread)
{
	if (_drawUpComplete.load() == _numUpdateThreads)
		return true;
	check(pThread);
	std::unique_lock<std::mutex> lock(_updateMutex);
	if (_updateModelIndex < _models.size())
	{
		/*for (CleanThread* pCleanThread : _cleanThreads)
		{
			std::unique_lock<std::mutex> lock_(pCleanThread->_mutex);
			if (pCleanThread->GetTask() == _models[_updateModelIndex] && pCleanThread->IsActive())
			{
				pCleanThread->Pause();
				break;
			}
		}*/

		pThread->SetTask(_models[_updateModelIndex]);
		_updateModelIndex++;
		pThread->WakeUp();
		return false;
	}
	else
	{
		_drawUpComplete++;
		if (_drawUpComplete.load() == _numUpdateThreads)
		{
			std::unique_lock <std::mutex> lock_(::GameThreadWaitFor);
			::GameThreadCondition.notify_all();
		}
		return true;
	}
}

void OsgbLoaderThreadPool::CleanModel(Model* model)
{
	std::unique_lock<std::mutex> lock(_cleanMutex);
	/*for (CleanThread* pCleanThread : _cleanThreads)
	{
		std::unique_lock<std::mutex> lock_(pCleanThread->_mutex);
		if (pCleanThread->GetTask() == model)
		{
			pCleanThread->WakeUp();
			return;
		}
	}*/

	for (auto itr = _waitForCleanModels.begin(); itr != _waitForCleanModels.end(); itr++)
		if (*itr == model)
			goto NoNeedForPush;
	_waitForCleanModels.push_back(model);
NoNeedForPush:
	for (CleanThread* pCleanThread : _cleanThreads)
	{
		// std::unique_lock<std::mutex> lock_(pCleanThread->_mutex);
		if (pCleanThread->IsActive() == false/* && pCleanThread->GetTask() == nullptr*/)
			pCleanThread->WakeUp();
	}
}
bool OsgbLoaderThreadPool::ReturnToPool(CleanThread* pThread)
{
	check(pThread);
	std::unique_lock<std::mutex> lock(_cleanMutex);
	if (_waitForCleanModels.empty())
		return true;
	else
	{
		// std::unique_lock<std::mutex> lock_(pThread->_mutex);
		pThread->SetTask(_waitForCleanModels.front());
		_waitForCleanModels.pop_front();
		// pThread->WakeUp();
		return false;
	}
}

void OsgbLoaderThreadPool::FinishStructureAndMountToModel(PagedLOD* plod)
{
	_waitToMountPlods.Enqueue(plod);
	for (StructureThread* pThread : _structureThread)
		if (pThread->IsActive() == false)
			//ReturnToPool(pThread);
			pThread->WakeUp();
}
bool OsgbLoaderThreadPool::ReturnToPool(StructureThread* pThread)
{
	check(pThread);
	if (_waitToMountPlods.IsEmpty())
		return true;
	else
	{
		PagedLOD* newTask = nullptr;
		_waitToMountPlods.Dequeue(newTask);
		pThread->SetTask(newTask);
		pThread->WakeUp();
		return false;
	}
}

void OsgbLoaderThreadPool::ReadNodeFile(FileReadTask* fileReadRequest)
{
	_fileReadRequestQueue.Enqueue(fileReadRequest);
	for (ReadThread* _pThread : _readThreads)
	{
		if (!_pThread->IsActive())
			//ReturnToPool(_pThread);
			_pThread->WakeUp();
	}
}
bool OsgbLoaderThreadPool::ReturnToPool(ReadThread* pThread)
{
	check(pThread);
	FileReadTask* fileReadRequest = nullptr;
	if (_fileReadRequestQueue.Dequeue(fileReadRequest))
	{
		pThread->SetTask(fileReadRequest);
		pThread->WakeUp();
		return false;
	}
	else
		return true;
}

UMaterialInterface* OsgbLoaderThreadPool::GetDefaultMaterial()
{
	return _runtimeMeshSubsystem->_mRuntimeMeshActor->_defaultMaterial;
}

AMyRuntimeMeshActor* OsgbLoaderThreadPool::GetRuntimeMeshActor()
{
	return _runtimeMeshSubsystem->_mRuntimeMeshActor;
}

void OsgbLoaderThreadPool::WaitForStart()
{
	std::unique_lock <std::mutex> lock(_initializeMutex);
	_initializeCondition.wait(lock);
}

void OsgbLoaderThreadPool::CleanView()
{
	for (Model* model : _models)
	{
		for (Geometry* geometry : model->_visibleGeometries)
		{
			geometry->_owner->MakeGeometryHide(geometry->_index);
		}
	}
}