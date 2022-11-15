// Fill out your copyright notice in the Description page of Project Settings.
#pragma warning(disable:4390)

#include "DatabasePager.h"
#include "PagedLOD_Deprecated.h"
#include "NodeVisitor_Deprecated.h"

#include <osgDB/ReaderWriter>
#include <osgDB/Options>
#include <osgDB/Registry>
#include <osgDB/FileCache>

#pragma region PagedLODList

class PagedLODList :public DatabasePager::PagedLODList_Interface
{
    typedef std::set<osg::observer_ptr<PagedLOD>> PagedLODs;
public:
    virtual PagedLODList_Interface* Clone() override
    {
        return new PagedLODList;
    }

    virtual void Clear() override
    {
        _pagedLODs.clear();
    }

    virtual unsigned int Size() override
    {
        return _pagedLODs.size();
    }

    virtual void RemoveExpiredChildren(int numberChildrenToRemove, FDateTime expiryTime, int64 expiryFrame,
        std::list<osg::ref_ptr<osg::Object>>& childrenRemoved, bool visitActive) override
    {
        int leftToRemove = numberChildrenToRemove;
        for (auto itr = _pagedLODs.begin();
            itr != _pagedLODs.end() && leftToRemove > 0;)
        {
            osg::ref_ptr<PagedLOD> plod;
            if (itr->lock(plod))
            {
                bool plodActive = expiryFrame < plod->getFrameNumberOfLastTraversal();
                if (visitActive == plodActive)
                {
                    ExpirePagedLODsVisitor expirePagedLODsVisitor;
                    osg::NodeList expiredChildren;
                    expirePagedLODsVisitor.RemoveExpiredChildrenAndFindPagedLODs(plod.get(), expiryTime, expiryFrame, expiredChildren);

                    for (auto itr_ = expirePagedLODsVisitor._childPagedLODs.begin(); itr_ != expirePagedLODsVisitor._childPagedLODs.end(); itr_++)
                    {
                        osg::observer_ptr<PagedLOD> obp_plod(*itr_);
                        if (_pagedLODs.erase(obp_plod) > 0)
                            leftToRemove--;
                    }
                    std::copy(expiredChildren.begin(), expiredChildren.end(), std::back_inserter(childrenRemoved));
                }
                itr++;
            }
            else
            {
                _pagedLODs.erase(itr++);
                leftToRemove--;
            }
        }
    }

    virtual void RemoveNodes(osg::NodeList& nodesToRemove) override
    {
        for (auto itr = nodesToRemove.begin(); itr != nodesToRemove.end(); itr++)
        {
            PagedLOD* plod = dynamic_cast<PagedLOD*>(itr->get());
            osg::observer_ptr<PagedLOD> obs_ptr(plod);
            auto plod_itr = _pagedLODs.find(obs_ptr);
            if (plod_itr != _pagedLODs.end())
                _pagedLODs.erase(plod_itr);
        }
    }

    virtual void InsertPagedLOD(const osg::observer_ptr<PagedLOD>& plod) override
    {
        if (_pagedLODs.count(plod) == 0)
            _pagedLODs.insert(plod);
    }

    virtual bool ContainsPagedLOD(const osg::observer_ptr<PagedLOD>& plod) const override
    {
        return _pagedLODs.count(plod) != 0;
    }

    virtual void TraverseActivePagedLODs(osg::NodeVisitor& nv) override
    {
        /*UPagedLodVisitor* plv = dynamic_cast<UPagedLodVisitor*>(&nv);
        if (plv)
        {
            for (auto itr = _pagedLODs.begin(); itr != _pagedLODs.end(); itr++)
            {
                osg::ref_ptr<PagedLOD> plod;
                if (itr->lock(plod))
                {
                    if (plv->validNodeMask(*plod))
                    {
                        plv->pushOntoNodePath(plod);
                        plv->apply(*plod);
                        plv->popFromNodePath();
                    }
                }
            }
            return;
        }

        UpdateVisitor* uv = dynamic_cast<UpdateVisitor*>(&nv);
        if (uv)
        {
            for (auto itr = _pagedLODs.begin(); itr != _pagedLODs.end(); itr++)
            {
                osg::ref_ptr<PagedLOD> plod;
                if (itr->lock(plod))
                {
                    if (plod->isActive())
                    {
                        int numChildren = plod->getNumChildren();
                        for (int i = 0; i < numChildren; i++)
                        {
                            osg::Node* node = plod->getChild(i);
                            node->accept(nv);
                        }
                    }
                }
            }
            return;
        }*/
    }

public:
    PagedLODs _pagedLODs;
};

#pragma endregion

DatabasePager::DatabasePager()
{
    _bStartThreadCalled = false;
    _bDone = false;
    _bAcceptNewRequests = true;
    _bDatabasePagerThreadPaused = false;

    const char* str = getenv("OSG_DATABASE_PAGER_GEOMETRY");
    if (!str) str = getenv("OSG_DATABASE_PAGER_DRAWABLE");
    _bDeleteRemovedSubgraphsInDatabaseThread = true;
    if ((str = getenv("OSG_DELETE_IN_DATABASE_THREAD")) != 0)
    {
        _bDeleteRemovedSubgraphsInDatabaseThread = strcmp(str, "yes") == 0 || strcmp(str, "YES") == 0 ||
            strcmp(str, "on") == 0 || strcmp(str, "ON") == 0;
    }

    _targetMaximumNumberOfPageLOD = 100;
    if ((str = getenv("OSG_MAX_PAGEDLOD")) != 0)
    {
        _targetMaximumNumberOfPageLOD = atoi(str);
        OSG_NOTICE << "_targetMaximumNumberOfPageLOD = " << _targetMaximumNumberOfPageLOD << std::endl;
    }

    resetStats();

    _fileReadRequestQueue = new ReadRequestQueue(this);
    _dataToMergeList = new RequestQueue(this);

    str = getenv("OSG_DATABASE_PAGER_PRIORITY");
    if (str)
    {
        if (strcmp(str, "DEFAULT") == 0)
            setSchedulePriority(OpenThreads::Thread::THREAD_PRIORITY_DEFAULT);
        else if (strcmp(str, "MIN") == 0)
            setSchedulePriority(OpenThreads::Thread::THREAD_PRIORITY_MIN);
        else if (strcmp(str, "LOW") == 0)
            setSchedulePriority(OpenThreads::Thread::THREAD_PRIORITY_LOW);
        else if (strcmp(str, "NOMINAL") == 0)
            setSchedulePriority(OpenThreads::Thread::THREAD_PRIORITY_NOMINAL);
        else if (strcmp(str, "HIGH") == 0)
            setSchedulePriority(OpenThreads::Thread::THREAD_PRIORITY_HIGH);
        else if (strcmp(str, "MAX") == 0)
            setSchedulePriority(OpenThreads::Thread::THREAD_PRIORITY_MAX);
    }

    _activePagedLODList = new PagedLODList;
    _affinity = OpenThreads::Affinity();
}

DatabasePager::DatabasePager(const DatabasePager& rhs)
{
    _bStartThreadCalled = false;
    _bDone = false;
    _bAcceptNewRequests = true;
    _bDatabasePagerThreadPaused = false;

    _bDeleteRemovedSubgraphsInDatabaseThread = rhs._bDeleteRemovedSubgraphsInDatabaseThread;
    _targetMaximumNumberOfPageLOD = rhs._targetMaximumNumberOfPageLOD;
    
    _fileReadRequestQueue = new ReadRequestQueue(this);
    _dataToMergeList = new RequestQueue(this);

    for (auto itr = rhs._databaseThreads.begin(); itr != rhs._databaseThreads.end(); itr++)
        _databaseThreads.push_back(new DatabaseThread(**itr, this));

    setProcessorAffinity(rhs._affinity);
    _activePagedLODList = rhs._activePagedLODList->Clone();
    resetStats();
}

static osg::ref_ptr<DatabasePager>& prototype()
{
    static osg::ref_ptr<DatabasePager> s_DatabasePager = new DatabasePager;
    return s_DatabasePager;
}

static DatabasePager* create()
{
    return DatabasePager::prototype().valid() ?
        DatabasePager::prototype()->clone() :
        new DatabasePager;
}

void DatabasePager::RequestNodeFile(const std::string& fileName, osg::NodePath& nodePath,
    float priority, const FDateTime timestamp, int64 frameNumber,
    osg::ref_ptr<osg::Referenced>& databaseRequestRef, const osg::Referenced* options)
{
    if (!_bAcceptNewRequests)   return;

    osgDB::Options* loadOptions = dynamic_cast<osgDB::Options*>(const_cast<osg::Referenced*>(options));
    if (!loadOptions)   loadOptions = osgDB::Registry::instance()->getOptions();

    if (nodePath.empty())
    {
        // TODO     LogWarning
        return;
    }

    osg::Group* group = nodePath.back()->asGroup();
    if (!group)
    {
        // TODO     LogWarning
        return;
    }

    osg::Node* terrain = nullptr;
    for (auto itr = nodePath.rbegin(); itr != nodePath.rend(); itr++)
        if ((*itr)->asTerrain())
            terrain = *itr;

    bool foundEntry = false;
    if (databaseRequestRef.valid())
    {
        DatabaseRequest* databaseRequest = dynamic_cast<DatabaseRequest*>(databaseRequestRef.get());
        bool requeue = false;
        if (databaseRequest)
        {
            OpenThreads::ScopedLock<OpenThreads::Mutex> drLock(_dr_mutex);
            if (databaseRequest->IsValid())
            {
                databaseRequest->_frameNumberLastRequest = frameNumber;
                databaseRequest->_timestampLastRequest = timestamp;
                databaseRequest->_priorityLastRequest = priority;
                databaseRequest->_numOfRequests++;

                foundEntry = true;

                if (databaseRequestRef->referenceCount() == 1)
                {
                    databaseRequest->_frameNumberLastRequest = frameNumber;
                    databaseRequest->_timestampLastRequest = timestamp;
                    databaseRequest->_priorityLastRequest = priority;
                    databaseRequest->_group = group;
                    databaseRequest->_terrain = terrain;
                    databaseRequest->_loadOptions = loadOptions;
                    databaseRequest->_objectCache = nullptr;
                    requeue = true;
                }
            }
            else
                databaseRequest = nullptr;
        }
        if (requeue)
            _fileReadRequestQueue->Add(databaseRequest);
    }

    if (!foundEntry)
    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_fileReadRequestQueue->_requestMutex);

        if (!databaseRequestRef.valid() || databaseRequestRef->referenceCount() == 1)
        {
            osg::ref_ptr<DatabaseRequest> databaseRequest = new DatabaseRequest;
            databaseRequest->_bValid = true;
            databaseRequest->_fileName = fileName;
            databaseRequest->_frameNumberFirstRequest = frameNumber;
            databaseRequest->_timestampFirstRequest = timestamp;
            databaseRequest->_priorityFirstRequest = priority;
            databaseRequest->_frameNumberLastRequest = frameNumber;
            databaseRequest->_timestampLastRequest = timestamp;
            databaseRequest->_priorityLastRequest = priority;
            databaseRequest->_group = group;
            databaseRequest->_terrain = terrain;
            databaseRequest->_loadOptions = loadOptions;
            databaseRequest->_objectCache = nullptr;

            databaseRequestRef = databaseRequest.get();
            _fileReadRequestQueue->Add(databaseRequest);
        }
    }

    if (!_bStartThreadCalled)
    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_run_mutex);

        if (!_bStartThreadCalled)
        {
            // start Thread
            if (_databaseThreads.empty())
                setUpThreads(osg::DisplaySettings::instance()->getNumOfDatabaseThreadsHint() - 1);

            _bStartThreadCalled = true;
            _bDone = false;

            for (auto itr = _databaseThreads.begin(); itr != _databaseThreads.end(); itr++)
                (*itr)->startThread();
        }
    }
}

int DatabasePager::Cancel()
{
    int result = 0;

    for (auto itr = _databaseThreads.begin(); itr != _databaseThreads.end(); itr++)
        (*itr)->setDone(true);

    _fileReadRequestQueue.release();

    for (auto itr = _databaseThreads.begin(); itr != _databaseThreads.end(); itr++)
        (*itr)->cancel();

    _bDone = true;
    _bStartThreadCalled = false;

    return result;
}

void DatabasePager::UpdateSceneGraph(const FDateTime timestamp, const int64 frameNumber)
{
    _frameNumber = frameNumber;
    RemoveExpiredSubgraphs(timestamp, frameNumber);
    addLoadedDataToSceneGraph(timestamp, frameNumber);
}

void DatabasePager::RemoveExpiredSubgraphs(const FDateTime timestamp, const int64 frameNumber)
{
    if (frameNumber == 0)
        return;

    size_t numPagedLODs = _activePagedLODList->Size();
    if (numPagedLODs <= _targetMaximumNumberOfPageLOD)
        return;

    int numToPrune = numPagedLODs - _targetMaximumNumberOfPageLOD;

    ObjectList childrenRemoved;
    _activePagedLODList->RemoveExpiredChildren(numToPrune, timestamp - FTimespan(1), frameNumber - 1, childrenRemoved, false);
    numToPrune = _activePagedLODList->Size() - _targetMaximumNumberOfPageLOD;
    if (numToPrune > 0)
        _activePagedLODList->RemoveExpiredChildren(numToPrune, timestamp - FTimespan(1), frameNumber - 1, childrenRemoved, true);

    if (!childrenRemoved.empty())
    {
        if (_bDeleteRemovedSubgraphsInDatabaseThread)
        {
            OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_fileReadRequestQueue->_requestMutex);
            _fileReadRequestQueue->_childrenToDeleteList.splice(_fileReadRequestQueue->_childrenToDeleteList.end(), childrenRemoved);
            _fileReadRequestQueue->UpdateBlock();
        }
        else
            childrenRemoved.clear();
    }
}

void DatabasePager::addLoadedDataToSceneGraph(const FDateTime timestamp, const int64 frameNumber)
{
    std::list<osg::ref_ptr<DatabaseRequest>> localFileLoadedList;
    _dataToMergeList->Swap(localFileLoadedList);

    for (auto itr = localFileLoadedList.begin(); itr != localFileLoadedList.end(); itr++)
    {
        DatabaseRequest* databaseRequest = itr->get();

        osg::ref_ptr<osg::Group> group;
        if (!databaseRequest->_bGroupExpired && databaseRequest->_group.lock(group))
        {
            if (osgDB::Registry::instance()->getSharedStateManager())
                osgDB::Registry::instance()->getSharedStateManager()->share(databaseRequest->_loadedModel.get());

            PagedLOD* plod = dynamic_cast<PagedLOD*>(group.get());
            if (plod)
            {
                int n = plod->getNumChildren();
                plod->setTimeStamp(n, timestamp);
                plod->setFrameNumber(n, frameNumber);
                plod->getDatabaseRequest(n) = nullptr;
            }

            plod->addChild(databaseRequest->_loadedModel.get());

            if (plod && !_activePagedLODList->ContainsPagedLOD(plod))
                registerPagedLODs(plod, timestamp, frameNumber);
            else
                registerPagedLODs(databaseRequest->_loadedModel.get(), timestamp, frameNumber);
                
            FTimespan timeToMerge = timestamp - databaseRequest->_timestampFirstRequest;
            if (timeToMerge < _minimumTimeToMergeTile) _minimumTimeToMergeTile = timeToMerge;
            if (timeToMerge > _maximumTimeToMergeTile) _maximumTimeToMergeTile = timeToMerge;

            _totalTimeToMergeTiles += timeToMerge;
            _numTilesMerges++;
        }

        if (databaseRequest->_objectCache.valid() && osgDB::Registry::instance()->getObjectCache())
        {
            // insert loaded model into Registry ObjectCache
            osgDB::Registry::instance()->getObjectCache()->addObjectCache(databaseRequest->_objectCache.get());
            databaseRequest->_objectCache->clear();
        }
        databaseRequest->_loadedModel = nullptr;
    }
}

void DatabasePager::registerPagedLODs(osg::Node* subgraph, const FDateTime timestamp, const int64 frameNumber)
{
    if (subgraph)
    {
        osg::Group* group = dynamic_cast<osg::Group*>(subgraph);
        if (group)
        {
            FindPagedLODVisitor fplv(*_activePagedLODList, frameNumber);
            for (unsigned int i = 0; i < group->getNumChildren(); i++)
            {
                osg::PagedLOD* osg_plod = dynamic_cast<osg::PagedLOD*>(group->getChild(i));
                if (osg_plod)
                {
                    PagedLOD* plod = new PagedLOD(*osg_plod);
                    group->removeChild(osg_plod);
                    group->addChild(plod);
                    if (fplv.validNodeMask(*plod)) 
                    { 
                        fplv.pushOntoNodePath(plod); 
                        fplv.apply(*plod); 
                        fplv.popFromNodePath(); 
                    }
                    // plod->accept(fplv);
                }
            }
        }
    }
}

void DatabasePager::resetStats()
{
    _minimumTimeToMergeTile = INT_MAX;
    _maximumTimeToMergeTile = -INT_MAX;
    _totalTimeToMergeTiles = 0.0;
    _numTilesMerges = 0;
}

void DatabasePager::clear()
{
    _fileReadRequestQueue->Clear();
    _dataToMergeList->Clear();
    _activePagedLODList->Clear();
}

void DatabasePager::setUpThreads(unsigned int num)
{
    _databaseThreads.clear();
    for (unsigned int i = 0; i < num; i++)
    {
        addDatabaseThread("HANDLE_ALL_REQUESTS");
    }
}

unsigned int DatabasePager::addDatabaseThread(const std::string& name)
{
    unsigned int pos = _databaseThreads.size();
    DatabaseThread* thread = new DatabaseThread(this, name);
    thread->setProcessorAffinity(_affinity);
    _databaseThreads.push_back(thread);
    if (_bStartThreadCalled)
        thread->startThread();
    return pos;
}

int DatabasePager::setSchedulePriority(OpenThreads::Thread::ThreadPriority priority)
{
    int result = 0;
    for (auto itr = _databaseThreads.begin(); itr != _databaseThreads.end(); itr++)
        result = (*itr)->setSchedulePriority(priority);
    return result;
}

void DatabasePager::setDatabasePagerThreadPause(bool pause)
{
    if (_bDatabasePagerThreadPaused == pause)
        return;

    _bDatabasePagerThreadPaused = pause;
    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_fileReadRequestQueue->_requestMutex);
        _fileReadRequestQueue->UpdateBlock();
    }
}

void DatabasePager::setProcessorAffinity(const OpenThreads::Affinity& affinity)
{
    _affinity = affinity;

    for (auto itr = _databaseThreads.begin(); itr != _databaseThreads.end(); ++itr)
        (*itr)->setProcessorAffinity(_affinity);
}

#pragma region DatabaseThread

DatabasePager::DatabaseThread::DatabaseThread(DatabasePager* pager, const std::string& name) :
    _done(false),
    _bActive(false),
    _pager(pager),
    _name(name)
{
}

DatabasePager::DatabaseThread::DatabaseThread(const DatabaseThread& dt, DatabasePager* pager) :
    _done(false),
    _bActive(false),
    _pager(pager),
    _name(dt._name)
{
}

void DatabasePager::DatabaseThread::run()
{
    bool firstTime = true;
    osg::ref_ptr<DatabasePager::ReadRequestQueue> readRequestQueue = _pager->_fileReadRequestQueue;
    do
    {
        _bActive = false;
        readRequestQueue->Block();
        if (_done)
            break;
        _bActive = true;

        DeleteChildren(readRequestQueue);
        LoadSubgraphs(readRequestQueue);

        if (firstTime)
        {
            OpenThreads::Thread::YieldCurrentThread();
            firstTime = false;
        }
    } while (!testCancel() && !_done);
}

int DatabasePager::DatabaseThread::cancel()
{
    int result = 0;
    if (isRunning())
    {
        setDone(true);
        _pager->_fileReadRequestQueue->Release();
        join();
    }
    return result;
}

void DatabasePager::DatabaseThread::DeleteChildren(osg::ref_ptr<ReadRequestQueue>& readRequestQueue)
{
    if (_pager->_bDeleteRemovedSubgraphsInDatabaseThread)
    {
        ObjectList deleteList;
        {
            OpenThreads::ScopedLock<OpenThreads::Mutex> lock(readRequestQueue->_requestMutex);
            if (!readRequestQueue->_childrenToDeleteList.empty())
            {
                deleteList.swap(readRequestQueue->_childrenToDeleteList);
                readRequestQueue->UpdateBlock();
            }
        }
    }
}

void DatabasePager::DatabaseThread::LoadSubgraphs(osg::ref_ptr<ReadRequestQueue>& readRequestQueue)
{
    osg::ref_ptr<DatabaseRequest> databaseRequest;
    readRequestQueue->PopFront(databaseRequest);

    bool readFromFileCache = false;

    osg::ref_ptr<osgDB::FileCache> fileCache = osgDB::Registry::instance()->getFileCache();
    osg::ref_ptr<osgDB::FileLocationCallback> fileLocationCallback = osgDB::Registry::instance()->getFileLocationCallback();
    osg::ref_ptr<osgDB::Options> dr_loadOptions;
    std::string fileName;
    int64 frameNumberLastRequest = -1;
    bool cacheNodes = false;
    if (databaseRequest.valid())
    {
        {
            OpenThreads::ScopedLock<OpenThreads::Mutex> drLock(_pager->_dr_mutex);
            dr_loadOptions = databaseRequest->_loadOptions.valid() ? databaseRequest->_loadOptions->cloneOptions() : new osgDB::Options;
            dr_loadOptions->setTerrain(databaseRequest->_terrain);
            dr_loadOptions->setParentGroup(databaseRequest->_group);
            fileName = databaseRequest->_fileName;
            frameNumberLastRequest = databaseRequest->_frameNumberLastRequest;
        }

        if (dr_loadOptions->getFileCache()) 
            fileCache = dr_loadOptions->getFileCache();
        if (dr_loadOptions->getFileLocationCallback()) 
            fileLocationCallback = dr_loadOptions->getFileLocationCallback();
        // disable the FileCache if the fileLocationCallback tells us that it isn't required for this request.
        if (fileLocationCallback.valid() && !fileLocationCallback->useFileCache()) 
            fileCache = 0;

        cacheNodes = (dr_loadOptions->getObjectCacheHint() & osgDB::Options::CACHE_NODES) != 0;
        if (cacheNodes)
        {
            // check the object cache to see if the file we want has already been loaded.
            osg::ref_ptr<osg::Object> objectFromCache = osgDB::Registry::instance()->getRefFromObjectCache(fileName);

            // if no object with fileName in ObjectCache then try the filename appropriate for fileCache
            if (objectFromCache.valid() &&
                (fileCache.valid() && fileCache->isFileAppropriateForFileCache(fileName)))
            {
                if (fileCache->existsInCache(fileName))
                    objectFromCache = osgDB::Registry::instance()->getRefFromObjectCache(fileCache->createCacheFileName(fileName));
            }

            osg::Node* modelFromCache = dynamic_cast<osg::Node*>(objectFromCache.get());
            if (modelFromCache)
            {
                // assign the cached model to the request
                {
                    OpenThreads::ScopedLock<OpenThreads::Mutex> drLock(_pager->_dr_mutex);
                    databaseRequest->_loadedModel = modelFromCache;
                }
                // move the request to the dataToMerge list so it can be merged during the update phase of the frame.
                {
                    OpenThreads::ScopedLock<OpenThreads::Mutex> listLock(_pager->_dataToMergeList->_requestMutex);
                    _pager->_dataToMergeList->Add(databaseRequest.get());
                    databaseRequest = nullptr;
                }
                return;
            }
            else
            {
                OpenThreads::ScopedLock<OpenThreads::Mutex> drLock(_pager->_dr_mutex);
                databaseRequest->_objectCache = new osgDB::ObjectCache;
                dr_loadOptions->setObjectCache(databaseRequest->_objectCache.get());
            }
        }

        // check if databaseRequest is still relevant
        if (_pager->_frameNumber - frameNumberLastRequest <= 1)
        {
            if (fileCache.valid() && fileCache->isFileAppropriateForFileCache(fileName))
            {
                if (fileCache->existsInCache(fileName))
                {
                    readFromFileCache = true;
                }
            }
        }
        else
            databaseRequest = nullptr;

        if (databaseRequest.valid())
        {
            osgDB::ReaderWriter::ReadResult rr = readFromFileCache ?
                fileCache->readNode(fileName, dr_loadOptions.get(), false) :
                osgDB::Registry::instance()->readNode(fileName, dr_loadOptions.get(), false);

            osg::ref_ptr<osg::Node> loadedModel;
            if (rr.validNode())
                loadedModel = rr.getNode();
            if (!rr.success());
                // TODO: LogError

            if (loadedModel.valid() && fileCache.valid() &&
                fileCache->isFileAppropriateForFileCache(fileName) && !readFromFileCache)
                fileCache->writeNode(*loadedModel, fileName, dr_loadOptions.get());

            {
                OpenThreads::ScopedLock<OpenThreads::Mutex> drLock(_pager->_dr_mutex);
                if ((_pager->_frameNumber - databaseRequest->_frameNumberLastRequest) > 1)
                {
                    //TODO: LogWarning
                    loadedModel = nullptr;
                }
            }

            // TODO:    GL ���룿

            {
                OpenThreads::ScopedLock<OpenThreads::Mutex> drLock(_pager->_dr_mutex);
                databaseRequest->_loadedModel = loadedModel;
            }
            {
                OpenThreads::ScopedLock<OpenThreads::Mutex> listLock(_pager->_dataToMergeList->_requestMutex);
                _pager->_dataToMergeList->Add(databaseRequest.get());
                databaseRequest = nullptr;
            }
        }
    }
    else
        OpenThreads::Thread::YieldCurrentThread();
}

#pragma endregion

#pragma region DatabaseRequest

DatabasePager::DatabaseRequest::DatabaseRequest() :
    osg::Referenced(true),
    _bValid(false),
    _frameNumberFirstRequest(0),
    _timestampFirstRequest(0.0),
    _priorityFirstRequest(0.f),
    _frameNumberLastRequest(0),
    _timestampLastRequest(0.0),
    _priorityLastRequest(0.0f),
    _numOfRequests(0),
    _bGroupExpired(false)
{
}

bool DatabasePager::DatabaseRequest::IsRequestCurrent(int KSL_frameCount) const
{
    return _bValid && KSL_frameCount - _frameNumberLastRequest <= 1;
}

void DatabasePager::DatabaseRequest::invalidate()
{
    _bValid = false;
    _loadedModel = nullptr;
    // _compileSet = nullptr;
    _objectCache = nullptr;
}

#pragma endregion

#pragma region RequestQueue

DatabasePager::RequestQueue::RequestQueue(DatabasePager* pager):
    _pager(pager),
    _frameNumberLastPruned(-1)
{
}

void DatabasePager::RequestQueue::Add(DatabasePager::DatabaseRequest* databaseRequest)
{
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_requestMutex);
    _requestList.push_back(databaseRequest);
    UpdateBlock();
}

void DatabasePager::RequestQueue::Remove(DatabasePager::DatabaseRequest* databaseRequest)
{
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_requestMutex);
    for (auto itr = _requestList.begin(); itr != _requestList.end(); itr++)
        if (itr->get() == databaseRequest)
        {
            _requestList.erase(itr);
            return;
        }
}

bool DatabasePager::RequestQueue::Empty()
{
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_requestMutex);
    return _requestList.empty();
}

void DatabasePager::RequestQueue::Swap(RequestList& requestList)
{
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_requestMutex);
    _requestList.swap(requestList);
}

void DatabasePager::RequestQueue::PopFront(osg::ref_ptr<DatabaseRequest>& dr)
{
    static auto compareRequestPriority = [](const osg::ref_ptr<DatabasePager::DatabaseRequest>& lhs, const osg::ref_ptr<DatabasePager::DatabaseRequest>& rhs) -> bool
    {
        if (lhs->_timestampLastRequest > rhs->_timestampLastRequest) return true;
        else if (lhs->_timestampLastRequest < rhs->_timestampLastRequest) return false;
        else return (lhs->_priorityLastRequest > rhs->_priorityLastRequest);
    };

    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_requestMutex);

    if (!_requestList.empty())
    {
        auto selected_itr = _requestList.end();
        int64 frameNumber = _pager->_frameNumber;

        for (auto itr = _requestList.begin(); itr != _requestList.end();)
        {
            OpenThreads::ScopedLock<OpenThreads::Mutex> drLock(_pager->_dr_mutex);
            if ((*itr)->IsRequestCurrent(frameNumber))
            {
                if (selected_itr == _requestList.end() || compareRequestPriority(*itr, *selected_itr))
                    selected_itr = itr;
                itr++;
            }
            else
            {
                invalidate(itr->get());
                itr = _requestList.erase(itr);
            }
        }

        _frameNumberLastPruned = frameNumber;
        if (selected_itr != _requestList.end())
        {
            dr = *selected_itr;
            _requestList.erase(selected_itr);
        }

        UpdateBlock();
    }
}

void DatabasePager::RequestQueue::invalidate(DatabaseRequest* dr)
{
    /*osg::ref_ptr<CompileSet> compileSet;
    if (dr->_compileSet.lock(compileSet) && _pager->getIncrementalCompileOperation())
        _pager->getIncrementalCompileOperation()->remove(compileSet.get());*/

    dr->invalidate();
}

void DatabasePager::RequestQueue::Clear()
{
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_requestMutex);

    for (auto itr = _requestList.begin(); itr != _requestList.end(); itr++)
    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> drLock(_pager->_dr_mutex);
        invalidate(itr->get());
    }

    _requestList.clear();
    _frameNumberLastPruned = _pager->_frameNumber;
    UpdateBlock();
}

DatabasePager::RequestQueue::~RequestQueue()
{
    for (auto itr = _requestList.begin(); itr != _requestList.end(); ++itr)
        invalidate(itr->get());
}

#pragma endregion

#pragma region ReadRequestQueue

DatabasePager::ReadRequestQueue::ReadRequestQueue(DatabasePager* pager):
    RequestQueue(pager)
{
    _block = new osg::RefBlock;
}

void DatabasePager::ReadRequestQueue::UpdateBlock()
{
    _block->set((!_requestList.empty() || !_childrenToDeleteList.empty()) &&
        !_pager->_bDatabasePagerThreadPaused);
}

#pragma endregion 