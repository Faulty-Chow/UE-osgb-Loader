// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include <osg/LOD>
#include <osg/PagedLOD>

/**
 * 
 */
class PagedLOD : public osg::LOD
{
public:
	class PerRangeData
	{
	public:
		PerRangeData();
		PerRangeData(const PerRangeData& prd);
		PerRangeData& operator = (const PerRangeData& prd);
	public:
		std::string _filename;
		float _priorityOffset;
		float _priorityScale;
		FDateTime _minExpiryTime;
		int64 _minExpiryFrames;
		FDateTime _timeStamp;
		int64 _frameNumber;
		int64 _frameNumberOfLastReleaseUObject;
		osg::ref_ptr<osg::Referenced> _databaseRequest;
	};

public:
	PagedLOD();
	PagedLOD(const PagedLOD&, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);
	PagedLOD(const osg::PagedLOD&, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);
	META_Node(osgbLoad, PagedLOD);

public:
	virtual void traverse(osg::NodeVisitor& nv) override;
	virtual bool RemoveExpiredChildren(FDateTime expiryTime, int64 expiryFrame, osg::NodeList& removedChildren);
	virtual bool addChild(Node* child) override;
	virtual bool addChild(Node* child, float rmin, float rmax) override;
	virtual bool addChild(Node* child, float rmin, float rmax, const std::string& filename, float priorityOffset = 0.0f, float priorityScale = 1.0f);
	virtual bool removeChildren(unsigned int pos, unsigned int numChildrenToRemove = 1) override;

public:
	template<class T> bool addChild(const osg::ref_ptr<T>& child, float rmin, float rmax) { return addChild(child.get(), rmin, rmax); }
	template<class T> bool addChild(const osg::ref_ptr<T>& child, float rmin, float rmax, const std::string& filename, 
		float priorityOffset = 0.0f, float priorityScale = 1.0f) { return addChild(child.get(), rmin, rmax, filename, priorityOffset, priorityScale); }
	
	void setDatabaseOptions(osg::Referenced* options) { _databaseOptions = options; }
	osg::Referenced* getDatabaseOptions() { return _databaseOptions.get(); }
	const osg::Referenced* getDatabaseOptions() const { return _databaseOptions.get(); }

	void setDatabasePath(const std::string& path);
	inline const std::string& getDatabasePath() const { return _databasePath; }

	osg::ref_ptr<osg::Referenced>& getDatabaseRequest(unsigned int childNo) { return _perRangeDataList[childNo]._databaseRequest; }
	const osg::ref_ptr<osg::Referenced>& getDatabaseRequest(unsigned int childNo) const { return _perRangeDataList[childNo]._databaseRequest; }

	inline void setFrameNumberOfLastTraversal(unsigned int frameNumber) { _frameNumberOfLastTraversal = frameNumber; }
	inline unsigned int getFrameNumberOfLastTraversal() const { return _frameNumberOfLastTraversal; }

	inline void setNumChildrenThatCannotBeExpired(unsigned int num) { _numChildrenThatCannotBeExpired = num; }
	unsigned int getNumChildrenThatCannotBeExpired() const { return _numChildrenThatCannotBeExpired; }

	void setDisableExternalChildrenPaging(bool flag) { _disableExternalChildrenPaging = flag; }
	bool getDisableExternalChildrenPaging() const { return _disableExternalChildrenPaging; }

	unsigned int getNumPerRangeDataList() const { return static_cast<unsigned int>(_perRangeDataList.size()); }

	void setFileName(unsigned int childNo, const std::string& filename) { expandPerRangeDataTo(childNo); _perRangeDataList[childNo]._filename = filename; }
	const std::string& getFileName(unsigned int childNo) const { return _perRangeDataList[childNo]._filename; }

	void setPriorityOffset(unsigned int childNo, float priorityOffset) { expandPerRangeDataTo(childNo); _perRangeDataList[childNo]._priorityOffset = priorityOffset; }
	float getPriorityOffset(unsigned int childNo) const { return _perRangeDataList[childNo]._priorityOffset; }

	void setPriorityScale(unsigned int childNo, float priorityScale) { expandPerRangeDataTo(childNo); _perRangeDataList[childNo]._priorityScale = priorityScale; }
	float getPriorityScale(unsigned int childNo) const { return _perRangeDataList[childNo]._priorityScale; }

	void setMinimumExpiryTime(unsigned int childNo, FDateTime minTime) { expandPerRangeDataTo(childNo); _perRangeDataList[childNo]._minExpiryTime = minTime; }
	FDateTime getMinimumExpiryTime(unsigned int childNo) const { return _perRangeDataList[childNo]._minExpiryTime; }

	void setMinimumExpiryFrames(unsigned int childNo, int64 minFrames) { expandPerRangeDataTo(childNo); _perRangeDataList[childNo]._minExpiryFrames = minFrames; }
	int64 getMinimumExpiryFrames(unsigned int childNo) const { return _perRangeDataList[childNo]._minExpiryFrames; }

	void setTimeStamp(unsigned int childNo, FDateTime timeStamp) { expandPerRangeDataTo(childNo); _perRangeDataList[childNo]._timeStamp = timeStamp; }
	FDateTime getTimeStamp(unsigned int childNo) const { return _perRangeDataList[childNo]._timeStamp; }

	void setFrameNumber(unsigned int childNo, int64 frameNumber) { expandPerRangeDataTo(childNo); _perRangeDataList[childNo]._frameNumber = frameNumber; }
	int64 getFrameNumber(unsigned int childNo) const { return _perRangeDataList[childNo]._frameNumber; }

protected:
	virtual ~PagedLOD() = default;
	void expandPerRangeDataTo(unsigned int pos) { if (pos >= _perRangeDataList.size()) _perRangeDataList.resize(pos + 1); }

protected:
	osg::ref_ptr<Referenced> _databaseOptions;
	std::string _databasePath;

	unsigned int _frameNumberOfLastTraversal;
	unsigned int _numChildrenThatCannotBeExpired;
	bool _disableExternalChildrenPaging;

	std::vector<PerRangeData> _perRangeDataList;
};
