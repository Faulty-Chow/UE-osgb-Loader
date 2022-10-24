#include "FileReadTask"

#include <osgDB/ReadFile>

//int32 FileReadTask::Execute()
//{
//	if (_bAbandon.load())
//		return 0;
//	_readResult = osgDB::readNodeFile(_filePath, new osgDB::Options("Compressor=zlib"));
//	if (_readResult == nullptr)
//		return 1;
//	return 0;
//}