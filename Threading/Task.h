
#ifndef TASK_H_INCLUDED
#define TASK_H_INCLUDED

#include "../Common.h"

#include "TaskMgr.h"

class SHTask : public IHostTask {
private:
   volatile LONG m_cRef;
   HANDLE m_hThread;
   DWORD m_nativeId;

   SHTaskManager *m_pTaskManager;
   ICLRTask *m_pCLRTask;

public:
   SHTask(SHTaskManager *pTaskManager, DWORD nativeThreadId, HANDLE hThread);
   SHTask(SHTaskManager *pTaskManager, DWORD nativeThreadId);
   virtual ~SHTask();

   HANDLE GetThreadHandle() { return m_hThread; };

   // IUnknown functions
   STDMETHODIMP_(DWORD) AddRef();
   STDMETHODIMP_(DWORD) Release();
   STDMETHODIMP QueryInterface(const IID &riid, void **ppvObject);

   // IHostTask functions
   STDMETHODIMP Start();
   STDMETHODIMP Alert();
   STDMETHODIMP Join(/* in */ DWORD dwMilliseconds, /* in */ DWORD dwOption);
   STDMETHODIMP SetPriority(/* in */ int newPriority);
   STDMETHODIMP GetPriority(/* out */ int *pPriority);
   STDMETHODIMP SetCLRTask(/* in */ ICLRTask *pCLRTask);
};

#endif //TASK_H_INCLUDED
