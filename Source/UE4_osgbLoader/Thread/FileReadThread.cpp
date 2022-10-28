#include "FileReadThread"

//bool FileReadThread::SetTask(void* task)
//{
//	if (!_bFinish.load())
//		return false;
//	_task = static_cast<FileReadTask*>(task);
//	_bFinish.exchange(_task != nullptr);
//	return _bFinish.load();
//}