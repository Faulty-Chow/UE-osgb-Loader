#include "BaseAsyncTask"

#include "../Database/PagedLOD"
#include "../Instance/Pawn"
#include "../Database/Model"

int32 CleanCache::Execute()
{
	try {
		DFS(_model->_root);
	}
	catch (.../*const char* msg*/)
	{
		UE_LOG(LogTemp, Error, TEXT("Clean task Abandon, clean %d PagedLODs"), _releaseNum);
		return 1;
	}
	UE_LOG(LogTemp, Error, TEXT("Clean task done, clean %d PagedLODs"), _releaseNum);
	return 0;
}

void CleanCache::DFS(PagedLOD* plod)
{
	if (_bAbandon.load())
		throw "Task is abandoned";
	if (plod == nullptr)
		return;
	if (Pawn::GetCurrentPawn()->GetFrameNumber() - plod->_lastFrameUsed > _recycleFrameout)
	{
		_releaseNum++;
		plod->Release();
	}
	for (PagedLOD* child : plod->_children)
		DFS(child);
	if (_bAbandon.load())
		throw "Task is abandoned";
}