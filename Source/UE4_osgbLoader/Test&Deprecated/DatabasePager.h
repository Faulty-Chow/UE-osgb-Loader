#pragma once

#include "CoreMinimal.h"
#include "PagedLOD_Deprecated.h"

#include <osg/NodeVisitor>
#include <osg/Group>

#include <osgDB/Options>
#include <osgDB/ObjectCache>

#include <osgUtil/IncrementalCompileOperation>

#include <OpenThreads/Thread>
#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>
#include <OpenThreads/Condition>

#include <string>
#include <list>

class DatabasePager : public osg::NodeVisitor::DatabaseRequestHandler
{
public:
    class DatabaseThread;
protected:
    class DatabaseRequest;
    class RequestQueue;
    class ReadRequestQueue;
private:
    friend class ExpirePagedLODsVisitor;
    typedef std::list<osg::ref_ptr<osg::Object>> ObjectList;
    typedef std::vector<osg::ref_ptr<DatabaseThread>> DatabaseThreadList;

public:
    class DatabaseThread : public osg::Referenced, public OpenThreads::Thread
    {
    public:
        DatabaseThread(DatabasePager* pager, const std::string& name);
        DatabaseThread(const DatabaseThread& dt, DatabasePager* pager);

        virtual void run() override;
        virtual int cancel() override;

        void setName(const std::string& name) { _name = name; }
        const std::string& getName() const { return _name; }

        void setDone(bool done) { _done.exchange(done ? 1 : 0); }
        bool getDone() const { return _done != 0; }

        void setActive(bool active) { _bActive = active; }
        bool getActive() const { return _bActive; }

    private:
        void DeleteChildren(osg::ref_ptr<ReadRequestQueue>&);
        void LoadSubgraphs(osg::ref_ptr<ReadRequestQueue>&);

    private:
        OpenThreads::Atomic _done;
        volatile bool _bActive;
        DatabasePager* _pager;
        std::string _name;
    };
    class PagedLODList_Interface :public osg::Referenced
    {
    public:
        virtual PagedLODList_Interface* Clone() = 0;
        virtual void Clear() = 0;
        virtual unsigned int Size() = 0;
        virtual void RemoveExpiredChildren(int numberChildrenToRemove, FDateTime expiryTime, int64 expiryFrame,
            std::list<osg::ref_ptr<osg::Object>>& childrenRemoved, bool visitActive) = 0;
        virtual void RemoveNodes(osg::NodeList& nodesToRemove) = 0;
        virtual void InsertPagedLOD(const osg::observer_ptr<PagedLOD>& plod) = 0;
        virtual bool ContainsPagedLOD(const osg::observer_ptr<PagedLOD>& plod) const = 0;
        virtual void TraverseActivePagedLODs(osg::NodeVisitor& nv) = 0;
    };

protected:
    class DatabaseRequest:public osg::Referenced
    {
        // typedef osgUtil::IncrementalCompileOperation::CompileSet CompileSet;
    public:
        DatabaseRequest();
        inline bool IsValid() const { return _bValid; }
        inline bool IsRequestCurrent(int KSL_frameCount) const;
        void invalidate();

    public:
        bool _bValid;
        std::string _fileName;
        int64 _frameNumberFirstRequest;
        FDateTime _timestampFirstRequest;
        float _priorityFirstRequest;
        int64 _frameNumberLastRequest;
        FDateTime _timestampLastRequest;
        float _priorityLastRequest;
        unsigned int _numOfRequests;

        osg::observer_ptr<osg::Node> _terrain;
        osg::observer_ptr<osg::Group> _group;

        osg::ref_ptr<osg::Node> _loadedModel;
        osg::ref_ptr<osgDB::Options> _loadOptions;
        osg::ref_ptr<osgDB::ObjectCache> _objectCache;

        // osg::observer_ptr<CompileSet> _compileSet;
        bool _bGroupExpired;    // flag used only in update thread
    };
    class RequestQueue : public osg::Referenced
    {
        typedef std::list<osg::ref_ptr<DatabaseRequest>> RequestList;
        // typedef osgUtil::IncrementalCompileOperation::CompileSet CompileSet;
    public:
        RequestQueue(DatabasePager* pager);
        void Add(DatabaseRequest* databaseRequest);
        void Remove(DatabaseRequest* databaseRequest);
        bool Empty();
        void Swap(RequestList& requestList);
        void PopFront(osg::ref_ptr<DatabaseRequest>& dr);
        void invalidate(DatabaseRequest* dr);
        void Clear();

        virtual void UpdateBlock() {};

    public:
        OpenThreads::Mutex _requestMutex;

        RequestList _requestList;
        DatabasePager* _pager;
        int64 _frameNumberLastPruned;

    protected:
        virtual ~RequestQueue(); 
    };
    class ReadRequestQueue :public RequestQueue
    {
    public:
        ReadRequestQueue(DatabasePager* pager);
        void Block() { _block->block(); }
        void Release() { _block->release(); }
        virtual void UpdateBlock() override;
    public:
        osg::ref_ptr<osg::RefBlock> _block;

        OpenThreads::Mutex          _childrenToDeleteListMutex;
        ObjectList                  _childrenToDeleteList;
    };

public:
    DatabasePager();
    DatabasePager(const DatabasePager& rhs);
    virtual const char* className() const { return "DatabasePager"; }
    virtual DatabasePager* clone() const { return new DatabasePager(*this); }
    static osg::ref_ptr<DatabasePager>& prototype();
    static DatabasePager* create();

public:
    virtual void requestNodeFile(const std::string& fileName, osg::NodePath& nodePath,
        float priority, const osg::FrameStamp* framestamp,
        osg::ref_ptr<osg::Referenced>& databaseRequest,
        const osg::Referenced* options) override {
        UE_LOG(LogTemp, Warning, TEXT("requestNodeFile is deprecated in DatabasePager, you can use RequestNodeFile(...) instead."));
    } 
    virtual void RequestNodeFile(const std::string& fileName, osg::NodePath& nodePath,
        float priority, const FDateTime timestamp, int64 frameNumber,
        osg::ref_ptr<osg::Referenced>& databaseRequestRef, const osg::Referenced* options);
    virtual int Cancel();
    virtual void UpdateSceneGraph(const FDateTime timestamp, const int64 frameNumber);
    virtual void registerPagedLODs(osg::Node* subgraph, const FDateTime timestamp, const int64 frameNumber);
    virtual void resetStats();
    virtual void clear();
    virtual void setUpThreads(unsigned int num);
    virtual unsigned int addDatabaseThread(const std::string& name);

public:
    int setSchedulePriority(OpenThreads::Thread::ThreadPriority priority);
    void setDatabasePagerThreadPause(bool pause);
    bool getDatabasePagerThreadPause() const { return _bDatabasePagerThreadPaused; }
    void setProcessorAffinity(const OpenThreads::Affinity& affinity);

protected:
    void RemoveExpiredSubgraphs(const FDateTime timestamp, const int64 frameNumber);
    void addLoadedDataToSceneGraph(const FDateTime timestamp, const int64 frameNumber);

public:
    OpenThreads::Affinity _affinity;
    OpenThreads::Mutex _run_mutex;
    OpenThreads::Mutex _dr_mutex;

    std::atomic<int64> _frameNumber;

    bool _bDone;
    bool _bAcceptNewRequests;
    bool _bStartThreadCalled;
    bool _bDatabasePagerThreadPaused;
    bool _bDeleteRemovedSubgraphsInDatabaseThread;

    osg::ref_ptr<ReadRequestQueue> _fileReadRequestQueue;
    osg::ref_ptr<RequestQueue> _dataToMergeList;
    osg::ref_ptr<PagedLODList_Interface> _activePagedLODList;

    unsigned int _targetMaximumNumberOfPageLOD;

    FTimespan _minimumTimeToMergeTile;
    FTimespan _maximumTimeToMergeTile;
    FTimespan _totalTimeToMergeTiles;
    unsigned int _numTilesMerges;

    DatabaseThreadList _databaseThreads;
};
