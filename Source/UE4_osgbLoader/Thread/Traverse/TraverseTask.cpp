#include "TraverseTask"
#include "../../Instance/Pawn"
#include "../../Database/PagedLOD"
#include "../../Database/Model"
#include "../../Database/Geometry"

UpdateModelTask::UpdateModelTask(Model* model) :
	_model(model)
{
	check(model);
	check(model->_addToViewList.empty());
	check(model->_removeFromViewList.empty());
}

int32 UpdateModelTask::Execute()
{
	check(Pawn::GetCurrentPawn()->IsVaild());
	Traverse(_model->_root);
	
	while (!_UnuseableGeometries.empty())
	{
		PagedLOD* predecessor = nullptr;
		FindUseablePredecessor((*_UnuseableGeometries.begin())->_owner, predecessor);
		if (predecessor->_bActive)
		{
			// 使用前驱替代者
			UsePredecessor:
			for (auto itr = _renderNextFrame.begin(); itr != _renderNextFrame.end();)
			{
				if (*predecessor << *(*itr))
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
			if (FindUseableSuccessors((*_UnuseableGeometries.begin())->_owner, successors))
			{
				// 使用后继替代者
				_UnuseableGeometries.erase(_UnuseableGeometries.begin());
				for (Geometry* geometry : successors)
					_renderNextFrame.insert(geometry);
			}
			else
				goto UsePredecessor;
		}
	}

	for (auto itr = _renderNextFrame.begin(); itr != _renderNextFrame.end(); itr++)
		if (_model->_visibleGeometries.count(*itr) == 0)
			_model->_addToViewList.emplace_back(*itr);
	for (auto itr = _model->_visibleGeometries.begin(); itr != _model->_visibleGeometries.end(); itr++)
		if (_renderNextFrame.count(*itr) == 0)
			_model->_removeFromViewList.emplace_back(*itr);
	return 0;
}

void UpdateModelTask::Traverse(PagedLOD* plod)
{
	//if (plod == nullptr)	return;
	//for (Geometry* geometry : plod->_geometries)
	//{
	//	float pixelInSize = Pawn::GetCurrentPawn()->GetBoundPixelSizeOnView(geometry->_boundingSphere);
	//	if (Pawn::GetCurrentPawn()->IsBoundOnView(geometry->_boundingSphere))
	//	{
	//		if (pixelInSize < geometry->_threshold)
	//		{
	//			plod->_lastFrameUsed = Pawn::GetCurrentPawn()->GetFrameNumber();
	//			if (plod->_bNeedReload)
	//			{
	//				_UnuseableGeometries.emplace_back(geometry);
	//				plod->Reload();
	//			}
	//			else
	//				_renderNextFrame.insert(geometry);
	//		}
	//		else
	//		{
	//			if (plod->_children[geometry->GetSelfIndex()] == nullptr)
	//			{
	//				plod->_lastFrameUsed = Pawn::GetCurrentPawn()->GetFrameNumber();
	//				if (plod->_bNeedReload)
	//					_UnuseableGeometries.emplace_back(geometry);
	//				else
	//					_renderNextFrame.insert(geometry);
	//				geometry->LoadSuccessor();
	//			}
	//			else
	//				Traverse(plod->_children[geometry->GetSelfIndex()]);
	//		}
	//	}
	//	else
	//	{
	//		// 预加载周围的
	//		if (pixelInSize < geometry->_threshold)
	//		{
	//			plod->_lastFrameUsed = Pawn::GetCurrentPawn()->GetFrameNumber();
	//			if (plod->_bNeedReload)
	//				plod->Reload();
	//		}
	//		else
	//		{
	//			if(plod->_children[geometry->GetSelfIndex()] == nullptr)
	//			{
	//				plod->_lastFrameUsed = Pawn::GetCurrentPawn()->GetFrameNumber();
	//				geometry->LoadSuccessor();
	//			}
	//			else
	//				Traverse(plod->_children[geometry->GetSelfIndex()]);
	//		}
	//	}
	//}
}

//void UpdateModelTask::UpdateActivePagedLOD(PagedLOD* plod)
//{
//	if (Pawn::GetCurrentPawn()->IsBoundOnView(plod->_boundingSphere))
//	{
//		float pixelInSize = Pawn::GetCurrentPawn()->GetBoundPixelSizeOnView(plod->_boundingSphere);
//		if (pixelInSize < plod->_threshold)
//		{
//			if (plod->GetPredecessor()->_owner->_bNeedReload)
//			{
//
//			}
//			else
//			{
//				_model->_
//			}
//		}
//		else
//		{
//			for (Geometry* geometry : plod->_geometries)
//			{
//				if (geometry->IsVisible())
//				{
//					float pixelInSize_ = Pawn::GetCurrentPawn()->GetBoundPixelSizeOnView(geometry->_boundingSphere);
//					if (pixelInSize_ > geometry->_threshold)
//					{
//						if (geometry->GetSuccessor() == nullptr)
//						{
//
//						}
//						else
//						{
//							if (geometry->GetSuccessor()->_bNeedReload)
//							{
//
//							}
//							else
//							{
//								_model->_removeFromViewList.emplace_back(geometry);
//								for (Geometry* geometry_ : geometry->GetSuccessor()->_geometries)
//									_model->_addToViewList.emplace_back(geometry_);
//							}
//						}
//					}
//				}
//			}
//		}
//	}
//	else
//	{
//
//	}
//}

void UpdateModelTask::Cull(PagedLOD* plod)
{
	//if (plod == nullptr)	return;
	//for (Geometry* geometry : plod->_geometries)
	//	geometry->RemoveFromView();
	//for (PagedLOD* child : plod->_children)
	//	Cull(child);
}

void UpdateModelTask::CheckGeometry()
{
	
}

bool UpdateModelTask::FindUseableSuccessors(PagedLOD* plod, std::vector<Geometry*>& successors)
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

void UpdateModelTask::FindUseablePredecessor(PagedLOD* plod, PagedLOD*& predecessor)
{
	if (plod == nullptr)	return;
	if (plod->_bNeedReload)
		FindUseablePredecessor(plod->_parent, predecessor);
	else
		predecessor = plod;
}
