#include "Model"
#include "Geometry"
#include "PagedLOD"
#include "../ThreadPool/RuntimeOsgbLoaderThreadPool"
#include "../Thread/FileReadThread"
#include "../Instance/Pawn"
#include "../Instance/RuntimeMeshSubsystem.h"

#ifdef _MSVC_LANG
#if _MSVC_LANG < 201703L
int32 MeshSection::SectionID = 0;
#endif
#endif

Geometry::Geometry(PagedLOD* owner, int32 index, float threshold, osg::BoundingSphere boundingSphere,
	std::vector<MeshSection*>* meshSections, std::string successorFilename) :
	_owner(owner), _threshold(threshold), _boundingSphere(boundingSphere),
	_meshSections(meshSections), _index(index)
{
	std::string filePath = _owner->_owner->_folderPath + "\\" + successorFilename;
	PagedLOD* successor = new PagedLOD(this);
	_fileReadRequest = new FileReadTask(successor, filePath);
}

void Geometry::LoadSuccessor()
{
	// UE_LOG(LogTemp, Warning, TEXT("Request ReadNodeFile: %s"), *FString(_fileReadRequest->_filePath.c_str()));
	// _fileReadRequest->_frameNumberLastRequest = Pawn::GetCurrentPawn()->GetFrameNumber();
	URuntimeMeshSubsystem::GetRuntimeMeshSubsystem()->RequestNodeFile(_fileReadRequest);
}

Geometry::~Geometry()
{
	_owner = nullptr;
	delete _fileReadRequest;
	for (MeshSection* meshSection : *_meshSections)
		delete meshSection;
	_meshSections->clear();
}