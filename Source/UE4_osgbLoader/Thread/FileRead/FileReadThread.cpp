#include "FileReadThread"

#include <osgDB/Options>
#include <osgDB/ReadFile>

//void FileReadThread::run()
//{
//	osg::ref_ptr<osgDB::Options> options = new osgDB::Options("Compressor=zlib");
//	osg::ref_ptr<FileReadTask> fileReadRequest;
//
//	do
//	{
//		{
//			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_queueMutex);
//			if (!_fileReadRequestQueue.empty())
//			{
//				fileReadRequest = _fileReadRequestQueue.front();
//				_fileReadRequestQueue.pop();
//			}
//		}
//		if (fileReadRequest.valid())
//		{
//			osg::ref_ptr<osg::Node> readResult = osgDB::readNodeFile(fileReadRequest->_filepath, options);
//			(*fileReadRequest->Func_FileReadResponse)(readResult);
//			fileReadRequest = nullptr;
//		}
//		else
//			OpenThreads::Thread::microSleep(100);
//	} while (!testCancel() && !_bDone);
//}