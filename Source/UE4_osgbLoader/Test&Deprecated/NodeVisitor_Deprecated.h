#pragma once

#include "CoreMinimal.h"
#include "PagedLOD_Deprecated.h"
#include "DatabasePager.h"

#include <osg/NodeVisitor>
#include <osg/Geode>
#include <osg/Billboard>
#include <osg/LOD>
#include <osg/Switch>
#include <osg/LightSource>
#include <osg/Transform>
#include <osg/Projection>
#include <osg/OccluderNode>
#include <osg/ScriptEngine>

#include <OpenThreads/ReadWriteMutex>

class FindPagedLODVisitor :public osg::NodeVisitor
{
public:
	FindPagedLODVisitor(DatabasePager::PagedLODList_Interface& pagedLODList,int64 frameNumber);
	virtual void apply(PagedLOD& plod);

public:
	DatabasePager::PagedLODList_Interface& _activePagedLODList;
    int64 _frameNumber;

protected:
	FindPagedLODVisitor& operator = (const FindPagedLODVisitor&) { return *this; }
};

class ExpirePagedLODsVisitor :public osg::NodeVisitor
{
	typedef std::set<osg::ref_ptr<PagedLOD> > PagedLODset;
public:
	ExpirePagedLODsVisitor();
	virtual void apply(PagedLOD& plod);
	bool RemoveExpiredChildrenAndFindPagedLODs(PagedLOD* plod, FDateTime expiryTime, int64 expiryFrame, osg::NodeList& removedChildren);

private:
	void MarkRequestsExpired(PagedLOD* plod);

public:
	PagedLODset _childPagedLODs;
};

//class UpdateVisitor : public osg::NodeVisitor
//{
//public:
//    class SectionData
//    {
//    public:
//        TArray<FVector> _vertices;
//        TArray<FVector> _normals;
//        TArray<int32> _tangents;
//        TArray<FColor> _vertexColors;
//        TArray<FVector2D> _UV;
//        TArray<int32> _triangles;
//    };
//    SectionData* _sectionData;
//
//    UpdateVisitor()
//        :osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
//    {
//        _sectionData = new SectionData;
//    }
//    virtual ~UpdateVisitor()
//    {
//        delete _sectionData;
//    }
//
//    virtual void reset();
//
//    virtual void apply(osg::Geode& node) override;
//};
