#include "Model"
#include "PagedLOD"
#include "Geometry"

Model::Model(std::string folderPath) :
	_folderPath(folderPath)
{
	_root = new PagedLOD(this);
	_frameNumberLastClean = -1;
}

void Model::UpdateGeometries()
{
	for (Geometry* geometry : _addToViewList)
	{
		geometry->_owner->MakeGeometryVisible(geometry->_index);
		check(_visibleGeometries.insert(geometry).second);
	}
	_addToViewList.clear();
	for (Geometry* geometry : _removeFromViewList)
	{
		geometry->_owner->MakeGeometryHide(geometry->_index);
		check(_visibleGeometries.erase(geometry) == 1);
	}
	_removeFromViewList.clear();
}

Model::~Model()
{
	delete _root;
}