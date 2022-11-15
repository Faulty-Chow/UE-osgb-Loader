// Fill out your copyright notice in the Description page of Project Settings.


#include "PagedLOD_Deprecated.h"
#include "NodeVisitor_Deprecated.h"
#include "DatabasePager.h"

#include <algorithm>

#pragma region PerRangeData

PagedLOD::PerRangeData::PerRangeData() :
	_priorityOffset(0.f),
	_priorityScale(1.f),
	_minExpiryTime(0),
	_minExpiryFrames(0),
	_timeStamp(0),
	_frameNumber(0),
    _frameNumberOfLastReleaseUObject(0)
{}

PagedLOD::PerRangeData::PerRangeData(const PerRangeData& prd) :
    _filename(prd._filename),
    _priorityOffset(prd._priorityOffset),
    _priorityScale(prd._priorityScale),
    _minExpiryTime(prd._minExpiryTime),
    _minExpiryFrames(prd._minExpiryFrames),
    _timeStamp(prd._timeStamp),
    _frameNumber(prd._frameNumber),
    _frameNumberOfLastReleaseUObject(prd._frameNumberOfLastReleaseUObject),
    _databaseRequest(prd._databaseRequest)
{}

PagedLOD::PerRangeData& PagedLOD::PerRangeData::operator = (const PerRangeData& prd)
{
    if (this == &prd) return *this;
    _filename = prd._filename;
    _priorityOffset = prd._priorityOffset;
    _priorityScale = prd._priorityScale;
    _timeStamp = prd._timeStamp;
    _frameNumber = prd._frameNumber;
    _frameNumberOfLastReleaseUObject = prd._frameNumberOfLastReleaseUObject;
    _databaseRequest = prd._databaseRequest;
    _minExpiryTime = prd._minExpiryTime;
    _minExpiryFrames = prd._minExpiryFrames;
    return *this;
}

#pragma endregion

PagedLOD::PagedLOD()
{
    _frameNumberOfLastTraversal = 0;
    _centerMode = USER_DEFINED_CENTER;
    _radius = -1;
    _numChildrenThatCannotBeExpired = 0;
    _disableExternalChildrenPaging = false;
}

PagedLOD::PagedLOD(const PagedLOD& plod, const osg::CopyOp& copyop) :
    LOD(plod, copyop),
    _databaseOptions(plod._databaseOptions),
    _databasePath(plod._databasePath),
    _frameNumberOfLastTraversal(plod._frameNumberOfLastTraversal),
    _numChildrenThatCannotBeExpired(plod._numChildrenThatCannotBeExpired),
    _disableExternalChildrenPaging(plod._disableExternalChildrenPaging),
    _perRangeDataList(plod._perRangeDataList)
{}

PagedLOD::PagedLOD(const osg::PagedLOD& plod, const osg::CopyOp& copyop):
    LOD(plod,copyop)
{
    _databasePath = plod.getDatabasePath();
    _frameNumberOfLastTraversal = 0;
    _numChildrenThatCannotBeExpired = 0;
    _disableExternalChildrenPaging = false;
    for (unsigned int i = 0; i < plod.getNumFileNames(); i++)
    {
        PerRangeData* pRangeData = new PerRangeData();
        pRangeData->_filename = plod.getFileName(i);
        _perRangeDataList.push_back(*pRangeData);
    }
}

void PagedLOD::setDatabasePath(const std::string& path)
{
    _databasePath = path;
    if (!_databasePath.empty())
    {
        char& lastCharacter = _databasePath[_databasePath.size() - 1];
        const char unixSlash = '/';
        const char winSlash = '\\';

        if (lastCharacter == winSlash)
        {
            lastCharacter = unixSlash;
        }
        else if (lastCharacter != unixSlash)
        {
            _databasePath += unixSlash;
        }

        /*
        #ifdef WIN32
                if (lastCharacter==unixSlash)
                {
                    lastCharacter = winSlash;
                }
                else if (lastCharacter!=winSlash)
                {
                    _databasePath += winSlash;
                }
        #else
                if (lastCharacter==winSlash)
                {
                    lastCharacter = unixSlash;
                }
                else if (lastCharacter!=unixSlash)
                {
                    _databasePath += unixSlash;
                }
        #endif
        */
    }
}

void PagedLOD::traverse(osg::NodeVisitor& nv)
{
    FindPagedLODVisitor* fplodVisitor = dynamic_cast<FindPagedLODVisitor*>(&nv);
    if (fplodVisitor)
        if (fplodVisitor->getTraversalMode() == osg::NodeVisitor::TraversalMode::TRAVERSE_ALL_CHILDREN)
            std::for_each(_children.begin(), _children.end(), osg::NodeAcceptOp(*fplodVisitor));

    //UPagedLodVisitor* plodVisitor = dynamic_cast<UPagedLodVisitor*>(&nv);
    //if (plodVisitor)
    //{
    //    bool viewInfoVaild;
    //    FDateTime timeStamp;
    //    int64 frameNumber;
    //    FVector viewOrigin;
    //    FMatrix viewProjectionMatrix;
    //    FVector2D viewSize;
    //    {
    //        UPagedLodVisitor::ViewInfo* viewInfo = UPagedLodVisitor::ViewInfo::GetInstance();
    //        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(viewInfo->_rwMutex);
    //        timeStamp = viewInfo->GetTimeStamp();
    //        frameNumber = viewInfo->GetFrameNumber();
    //        viewOrigin = viewInfo->GetViewOrigin();
    //        viewProjectionMatrix = viewInfo->GetProjectionMatrix();
    //        viewSize = viewInfo->GetViewSize(true);
    //        viewInfoVaild = viewInfo->IsVaild();
    //    }

    //    setFrameNumberOfLastTraversal(frameNumber);
    //    if (!viewInfoVaild) return;

    //    if (plodVisitor->getTraversalMode() == osg::NodeVisitor::TraversalMode::TRAVERSE_ACTIVE_CHILDREN)
    //    {
    //        float required_range = 0;
    //        auto center = getCenter();
    //        FVector boundCenter(center.x(), center.y(), center.z());
    //        if (_rangeMode == RangeMode::DISTANCE_FROM_EYE_POINT)
    //            required_range = FVector::Distance(boundCenter, viewOrigin);
    //        else
    //        {
    //            float boundViewScale = plodVisitor->CalculateBoundViewScale(boundCenter, getBound().radius(), viewOrigin, viewProjectionMatrix);
    //            required_range = viewSize.X * viewSize.Y * boundViewScale;
    //        }

    //        int lastChildTraversed = -1;
    //        bool needToLoadChild = false;
    //        for (unsigned int i = 0; i < _rangeList.size(); i++)
    //        {
    //            if (_rangeList[i].first <= required_range && required_range < _rangeList[i].second)
    //            {
    //               if (i < _children.size())
    //                {
    //                    _perRangeDataList[i]._timeStamp = timeStamp;
    //                    _perRangeDataList[i]._frameNumber = frameNumber;
    //                    _children[i]->accept(*plodVisitor);
    //                    lastChildTraversed = (int)i;
    //                }
    //                else
    //                    needToLoadChild = true;
    //            }
    //        }

    //        if (needToLoadChild)
    //        {
    //            unsigned int numChildren = _children.size();
    //            // select the last valid child.
    //            if (numChildren > 0 && ((int)numChildren - 1) != lastChildTraversed)
    //            {
    //                _perRangeDataList[numChildren - 1]._timeStamp = timeStamp;
    //                _perRangeDataList[numChildren - 1]._frameNumber = frameNumber;
    //                _children[numChildren - 1]->accept(*plodVisitor);
    //            }

    //            // now request the loading of the next unloaded child.
    //            if (!_disableExternalChildrenPaging &&
    //                plodVisitor->getDatabaseRequestHandler() &&
    //                numChildren < _perRangeDataList.size())
    //            {
    //                float priority = (_rangeList[numChildren].second - required_range) / (_rangeList[numChildren].second - _rangeList[numChildren].first);
    //                if (_rangeMode == PIXEL_SIZE_ON_SCREEN)
    //                    priority = -priority;
    //                priority = _perRangeDataList[numChildren]._priorityOffset + priority * _perRangeDataList[numChildren]._priorityScale;

    //                std::string fileName;
    //                if (_databasePath.empty())
    //                    fileName = _perRangeDataList[numChildren]._filename;
    //                else
    //                    fileName = _databasePath + _perRangeDataList[numChildren]._filename;
    //                auto drHandler = plodVisitor->getDatabaseRequestHandler();
    //                DatabasePager* pager = dynamic_cast<DatabasePager*>(drHandler);
    //                if (pager)
    //                {
    //                    pager->RequestNodeFile(fileName, plodVisitor->getNodePath(),
    //                        priority, timeStamp, frameNumber,
    //                        _perRangeDataList[numChildren]._databaseRequest, _databaseOptions.get());
    //                }
    //                else;
    //                // TODO:    LogError
    //            }
    //        }
    //    }
    //}
}

bool PagedLOD::addChild(Node* child)
{
    if (LOD::addChild(child))
    {
        expandPerRangeDataTo(_children.size() - 1);
        return true;
    }
    return false;
}

bool PagedLOD::addChild(Node* child, float min, float max)
{
    if (LOD::addChild(child, min, max))
    {
        expandPerRangeDataTo(_children.size() - 1);
        return true;
    }
    return false;
}

bool PagedLOD::addChild(Node* child, float min, float max, const std::string& filename, float priorityOffset, float priorityScale)
{
    if (LOD::addChild(child, min, max))
    {
        setFileName(_children.size() - 1, filename);
        setPriorityOffset(_children.size() - 1, priorityOffset);
        setPriorityScale(_children.size() - 1, priorityScale);
        return true;
    }
    return false;
}

bool PagedLOD::removeChildren(unsigned int pos, unsigned int numChildrenToRemove)
{
    if (pos < _rangeList.size()) _rangeList.erase(_rangeList.begin() + pos, osg::minimum(_rangeList.begin() + (pos + numChildrenToRemove), _rangeList.end()));
    if (pos < _perRangeDataList.size()) _perRangeDataList.erase(_perRangeDataList.begin() + pos, osg::minimum(_perRangeDataList.begin() + (pos + numChildrenToRemove), _perRangeDataList.end()));

    return Group::removeChildren(pos, numChildrenToRemove);
}

bool PagedLOD::RemoveExpiredChildren(FDateTime expiryTime, int64 expiryFrame, osg::NodeList& removedChildren)
{
    if (_children.size() > _numChildrenThatCannotBeExpired)
    {
        unsigned int i = 0 /*_children.size() - 1*/;
        if (!_perRangeDataList[i]._filename.empty() &&
            _perRangeDataList[i]._timeStamp + _perRangeDataList[i]._minExpiryTime < expiryTime &&
            _perRangeDataList[i]._frameNumber + _perRangeDataList[i]._minExpiryFrames < expiryFrame)
        {
            osg::Node* nodeToRemove = _children[i].get();
            removedChildren.push_back(nodeToRemove);
            return Group::removeChildren(i, 1);
        }
    }
    return false;
}