#include "NodeVisitor"
#include "Geometry"
#include "PagedLOD"
#include "../Instance/OsgbLoaderThreadPool"

#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Texture2D>
#include <osg/TriangleIndexFunctor>
#include <osg/PagedLOD>

PagedLODVisitor::PagedLODVisitor(PagedLOD* newPagedLOD, bool bReload/* = false*/) :
	osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN),
	_plod(newPagedLOD), _bReload(bReload), _index(0)
{
}

void PagedLODVisitor::apply(osg::PagedLOD& plod)
{
	float threshold = plod.getRangeList().begin()->second;
	std::string filename;
	if (plod.getNumFileNames() == 2)
		filename = plod.getFileName(1);
	GeometryVisitor gv;
	plod.accept(gv);
	if (_bReload)
	{
		for (int32 i = 0; i < gv._meshSections->size(); i++)
		{
			Swap_ptr(gv._meshSections->operator[](i)->_vertices,
				_plod->_geometries[_index]->_meshSections->operator[](i)->_vertices);
			Swap_ptr(gv._meshSections->operator[](i)->_triangles,
				_plod->_geometries[_index]->_meshSections->operator[](i)->_triangles);
			Swap_ptr(gv._meshSections->operator[](i)->_normals,
				_plod->_geometries[_index]->_meshSections->operator[](i)->_normals);
			Swap_ptr(gv._meshSections->operator[](i)->_vertexColors,
				_plod->_geometries[_index]->_meshSections->operator[](i)->_vertexColors);
			Swap_ptr(gv._meshSections->operator[](i)->_UV,
				_plod->_geometries[_index]->_meshSections->operator[](i)->_UV);
			Swap_ptr(gv._meshSections->operator[](i)->_tangent,
				_plod->_geometries[_index]->_meshSections->operator[](i)->_tangent);
		}
		gv.ReloadClean();
	}
	else
	{
		Geometry* geometry = new Geometry(_plod, _index, threshold, gv._boundingSphere, gv._meshSections, filename);
		_plod->_geometries.emplace_back(geometry);
		_plod->_children.emplace_back(nullptr);
	}
	_index++;
}

GeometryVisitor::GeometryVisitor() :
	osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
{
	_meshSections = new std::vector<MeshSection*>;
}

void GeometryVisitor::Triangle::operator()(const unsigned int& v1, const unsigned int& v2, const unsigned int& v3)
{
	if (v1 == v2 || v1 == v3 || v2 == v3)
		return;
	_meshSection->_triangles->Add(v1);
	_meshSection->_triangles->Add(v2);
	_meshSection->_triangles->Add(v3);
}

void GeometryVisitor::apply(osg::Geode& geode)
{
	_boundingSphere = geode.getBound();
	for (size_t t = 0; t < geode.getNumDrawables(); t++)
	{
		MeshSection* meshSection = new MeshSection;
		int indexOffset = meshSection->_vertices->Num();
		osg::ref_ptr<osg::Geometry> drawable = dynamic_cast<osg::Geometry*>(geode.getDrawable(t));
		if (drawable.valid())
		{
			osg::ref_ptr<osg::Vec3Array> vertexArray = dynamic_cast<osg::Vec3Array*>(drawable->getVertexArray());
			osg::ref_ptr<osg::Vec3Array> normalArray = dynamic_cast<osg::Vec3Array*>(drawable->getNormalArray());
			osg::ref_ptr<osg::Texture2D> texture = dynamic_cast<osg::Texture2D*>(drawable->getStateSet()->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
			osg::ref_ptr<osg::Image> image = texture ? texture->getImage() : nullptr;

			if (image)
			{
				meshSection->_rows = image->t();
				meshSection->_cols = image->s();
				int channels = 0;
				switch (image->getPixelFormat())
				{
				case 6407:
					channels = 3;
					break;
				case 6408:
					channels = 4;
					break;
				case 6409:
					channels = 1;
					break;
				}
				meshSection->_textureData= new unsigned char[meshSection->_rows * meshSection->_cols * 4];
				check(meshSection->_textureData);

				int pSrc = 0, pDst = 0;
				unsigned char* data = static_cast<unsigned char*>(meshSection->_textureData);
				for (int y = 0; y < meshSection->_rows; y++)
				{
					for (int x = 0; x < meshSection->_cols; x++)
					{
						unsigned char r = image->data()[pSrc++];
						unsigned char g = image->data()[pSrc++];
						unsigned char b = image->data()[pSrc++];
						unsigned char a;
						if (channels == 4)
							a = image->data()[pSrc++];
						else
							a = UCHAR_MAX;
						data[pDst++] = b;
						data[pDst++] = g;
						data[pDst++] = r;
						data[pDst++] = a;
					}
				}
			}

			osg::ref_ptr<osg::Vec2Array> vertsUV = dynamic_cast<osg::Vec2Array*>(drawable->getTexCoordArray(0));
			if (vertexArray && vertsUV && vertexArray->size() != vertsUV->size())
			{
				UE_LOG(LogTemp, Error, TEXT("UVs can't match to Vertexes."));
				return;
			}

			for (int i = 0; i < vertexArray->size(); i++)
			{
				meshSection->_vertices->Emplace(100 * FVector{ -vertexArray->operator[](i)._v[0],vertexArray ->operator[](i)._v[1] ,vertexArray ->operator[](i)._v[2] });
				meshSection->_UV->Emplace(FVector2D{ vertsUV->operator[](i)._v[0], vertsUV->operator[](i)._v[1] });
				if (image)
				{
					osg::Vec4 color = image->getColor({ vertsUV->operator[](i)._v[0], vertsUV->operator[](i)._v[1] });
					meshSection->_vertexColors->Emplace(FColor{ (uint8)(color.r() * 255),(uint8)(color.g() * 255),(uint8)(color.b() * 255) });
				}
				if (normalArray && i < normalArray->size())
					meshSection->_normals->Emplace(FVector{ normalArray->operator[](i)._v[0],normalArray ->operator[](i)._v[1] ,normalArray ->operator[](i)._v[2] });
			}

			osg::TriangleIndexFunctor<Triangle> triangleIndex;
			triangleIndex._meshSection = meshSection;
			drawable->accept(triangleIndex);
			_meshSections->emplace_back(meshSection);
		}
	}
}

void GeometryVisitor::ReloadClean()
{
	for (MeshSection* meshSection : *_meshSections)
		delete meshSection;
	_meshSections->clear();
	delete _meshSections;
}