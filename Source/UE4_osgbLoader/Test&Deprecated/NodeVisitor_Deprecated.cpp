#include "NodeVisitor_Deprecated.h"
#include <osg/observer_ptr>
#include <osg/Geometry>
#include <osg/Texture2D>
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DefaultPawn.h"

#pragma region FindPagedLODVisitor

FindPagedLODVisitor::FindPagedLODVisitor(DatabasePager::PagedLODList_Interface& pagedLODList, int64 frameNumber):
	osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN),
	_activePagedLODList(pagedLODList),
	_frameNumber(frameNumber)
{
}

void FindPagedLODVisitor::apply(PagedLOD& plod)
{
	plod.setFrameNumberOfLastTraversal(_frameNumber);

	osg::observer_ptr<PagedLOD> obs_ptr(&plod);
	_activePagedLODList.InsertPagedLOD(obs_ptr);

	traverse(plod);
}

#pragma endregion

#pragma region ExpirePagedLODsVisitor

ExpirePagedLODsVisitor::ExpirePagedLODsVisitor():
	osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
{
}

void ExpirePagedLODsVisitor::apply(PagedLOD& plod)
{
	_childPagedLODs.insert(&plod);
	MarkRequestsExpired(&plod);
	traverse(plod);
}

bool ExpirePagedLODsVisitor::RemoveExpiredChildrenAndFindPagedLODs(PagedLOD* plod, FDateTime expiryTime, int64 expiryFrame, osg::NodeList& removedChildren)
{
	size_t sizeBefore = removedChildren.size();
	plod->RemoveExpiredChildren(expiryTime, expiryFrame, removedChildren);
	for (size_t i = sizeBefore; i < removedChildren.size(); i++)
		removedChildren[i]->accept(*this);
	return sizeBefore != removedChildren.size();
}

void ExpirePagedLODsVisitor::MarkRequestsExpired(PagedLOD* plod)
{
	unsigned int numFiles = plod->getNumPerRangeDataList();
	for (unsigned int i = 0; i < numFiles; i++)
	{
		DatabasePager::DatabaseRequest* request = dynamic_cast<DatabasePager::DatabaseRequest*>(plod->getDatabaseRequest(i).get());
		if (request)
			request->_bGroupExpired = true;
	}
}

#pragma endregion

//void UpdateVisitor::reset()
//{
//	_sectionData->_vertices.Empty();
//	_sectionData->_normals.Empty();
//	_sectionData->_tangents.Empty();
//	_sectionData->_UV.Empty();
//	_sectionData->_vertexColors.Empty();
//	_sectionData->_triangles.Empty();
//}
//
//void UpdateVisitor::apply(osg::Geode& node)
//{
//	for (size_t t = 0; t < node.getNumDrawables(); t++)
//	{
//		osg::ref_ptr<osg::Geometry> geometry = dynamic_cast<osg::Geometry*>(node.getDrawable(t));
//		if (geometry.valid())
//		{
//			osg::Vec3Array* vertexArray = dynamic_cast<osg::Vec3Array*>(geometry->getVertexArray());
//			osg::Vec3Array* normalArray = dynamic_cast<osg::Vec3Array*>(geometry->getNormalArray());
//			osg::Texture2D* tex2D = dynamic_cast<osg::Texture2D*>(geometry->getStateSet()->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
//			osg::Image* image = tex2D ? tex2D->getImage() : nullptr;
//			osg::Vec2Array* vertsUV = dynamic_cast<osg::Vec2Array*>(geometry->getTexCoordArray(0));
//			if (vertexArray && vertsUV && vertexArray->size() != vertsUV->size())
//			{
//				UE_LOG(LogTemp, Error, TEXT("UVs can't match to Vertexes."));
//				return;
//			}
//
//			for (int i = 0; i < vertexArray->size(); i++)
//			{
//				_sectionData->_vertices.Emplace(FVector{ vertexArray->operator[](i)._v[0],vertexArray ->operator[](i)._v[1] ,vertexArray ->operator[](i)._v[2] });
//				_sectionData->_UV.Emplace(FVector2D{ vertsUV->operator[](i)._v[0], vertsUV->operator[](i)._v[1] });
//				osg::Vec4 color = image->getColor({ vertsUV->operator[](i)._v[0], vertsUV->operator[](i)._v[1] });
//				_sectionData->_vertexColors.Emplace(FColor{ (uint8)(color.x() * 255),(uint8)(color.y() * 255),(uint8)(color.z() * 255),(uint8)(color.w() * 255) });
//				if (normalArray && i < normalArray->size())
//					_sectionData->_normals.Emplace(FVector{ normalArray->operator[](i)._v[0],normalArray ->operator[](i)._v[1] ,normalArray ->operator[](i)._v[2] });
//			}
//
//			unsigned int numP = geometry->getNumPrimitiveSets();
//			for (unsigned int ipr = 0; ipr < numP; ipr++)
//			{
//				osg::PrimitiveSet* prset = geometry->getPrimitiveSet(ipr);
//				unsigned int ncnt = prset->getNumIndices();
//				for (unsigned int ic = 0; ic * 3 < prset->getNumIndices(); ic++)
//				{
//					unsigned int iIndex0 = prset->index(ic * 3);
//					unsigned int iIndex1 = prset->index(ic * 3 + 1);
//					unsigned int iIndex2 = prset->index(ic * 3 + 2);
//					_sectionData->_triangles.Emplace(iIndex0);
//					_sectionData->_triangles.Emplace(iIndex1);
//					_sectionData->_triangles.Emplace(iIndex2);
//				}
//			}
//		}
//	}
//}