#include "RTDispatch.h"
#include "RTJSBuffer.h"
#include "atomic.h"

RTDispatch::RTDispatch()
: Task()
, fTimeoutTask(NULL, 120 * 1000)
, fTickTime(0)
{
	ListZero(&m_listSend);
	ListZero(&m_listPush);
	fTimeoutTask.SetTask(this);
}

RTDispatch::~RTDispatch(void)
{
	ListEmpty(&m_listPush);
	ListEmpty(&m_listSend);
}

int RTDispatch::SendData(const char*pData, int nLen)
{
    if (nLen > 9999) {
        LE("RTDispatch::SendData pData is over length\n");
        return -1;
    }
    {
        char* ptr = new char[nLen+1];
        memcpy(ptr, pData, nLen);
        ptr[nLen] = '\0';
        {
            OSMutexLocker locker(&mMutexSend);
            ListAppend(&m_listSend, ptr, nLen);
        }
    }

	this->Signal(kWriteEvent);
	return nLen;
}

int RTDispatch::PushData(const char*pData, int nLen)
{
    if (nLen > 9999) {
        LE("RTDispatch::PushData pData is over length\n");
        return -1;
    }
    {
        char* ptr = new char[nLen+1];
        memcpy(ptr, pData, nLen);
        ptr[nLen] = '\0';
        {
            OSMutexLocker locker(&mMutexPush);
            ListAppend(&m_listPush, ptr, nLen);
        }
    }
    
    this->Signal(kPushEvent);
    return nLen;
}

SInt64 RTDispatch::Run()
{
	EventFlags events = this->GetEvents();
	this->ForceSameThread();

	// Http session is short connection, need to kill session when occur TimeoutEvent.
	// So return -1.
	if(events&Task::kTimeoutEvent || events&Task::kKillEvent)
	{
        if (events&Task::kTimeoutEvent) {
            UpdateTimer();
        }
		return 0;
	}

	while(1)
	{
		if(events&Task::kReadEvent)
		{
            //OnRecvData("", 0);
			events -= Task::kReadEvent;
		}
		else if(events&Task::kWriteEvent)
		{
            ListElement *elem = NULL;
            if((elem = m_listSend.first) != NULL)
            {
                OnSendEvent((char*)elem->content, elem->size);
                {
                    OSMutexLocker locker(&mMutexSend);
                    ListRemoveHead(&m_listSend);
                }
                if(NULL != m_listSend.first)
                    this->Signal(kWriteEvent);
            }
			events -= Task::kWriteEvent;
		}
		else if(events&Task::kWakeupEvent)
		{
			OnWakeupEvent("", 0);
			events -= Task::kWakeupEvent;
		}
		else if(events&Task::kPushEvent)
		{
            ListElement *elem = NULL;
            if((elem = m_listPush.first) != NULL)
            {
                OnPushEvent((char*)elem->content, elem->size);
                {
                    OSMutexLocker locker(&mMutexPush);
                    ListRemoveHead(&m_listPush);
                }
                if(NULL != m_listPush.first)
                    this->Signal(kPushEvent);
            }
			events -= Task::kPushEvent;
		}
		else if(events&Task::kIdleEvent)
		{
			OnTickEvent("", 0);
			events -= Task::kIdleEvent; 
		}
		else
		{
			return fTickTime;
		}
	}
    return 0;
}