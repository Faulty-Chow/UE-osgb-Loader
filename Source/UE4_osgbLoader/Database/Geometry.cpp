#include "Model"
#include "Geometry"
#include "PagedLOD"
#include "../Instance/OsgbLoaderThreadPool"
#include "../Instance/Pawn"

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
	_fileReadRequest = new FileReadTask(this, filePath);
}

void Geometry::LoadSuccessor()
{
	//UE_LOG(LogTemp, Warning, TEXT("Request ReadNodeFile: %s"), *FString(_fileReadRequest->_filePath.c_str()));
	_fileReadRequest->_frameNumberLastRequest = Pawn::GetCurrentPawn()->GetFrameNumber();
	OsgbLoaderThreadPool::GetInstance()->ReadNodeFile(_fileReadRequest);
}

Geometry::~Geometry()
{
	_owner = nullptr;
	delete _fileReadRequest;
	for (MeshSection* meshSection : *_meshSections)
		delete meshSection;
	_meshSections->clear();
}