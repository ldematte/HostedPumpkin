
#ifndef TASK_MANAGER_H_INCLUDED
#define TASK_MANAGER_H_INCLUDED

#include "../Common.h"
#include "../HostContext.h"

#include <map>


class SHTaskManager : public IHostTaskManager {
private:
   volatile LONG m_cRef;
   ICLRTaskManager *m_pCLRTaskManager;
   HostContext* hostContext;
   
   LPCRITICAL_SECTION nativeThreadMapCrst;
   std::map<DWORD, IHostTask*> nativeThreadMap;
   
   LPCRITICAL_SECTION managedThreadMapCrst;
   std::map<DWORD, ICLRTask*> managedThreadMap;

public:
   SHTaskManager(HostContext* hostContext);
   ~SHTaskManager();

   // IUnknown functions
   STDMETHODIMP_(DWORD) AddRef();
   STDMETHODIMP_(DWORD) Release();
   STDMETHODIMP QueryInterface(const IID &riid, void **ppvObject);

   // IHostTaskManager functions
   STDMETHODIMP GetCurrentTask(/* out */ IHostTask **pTask);
   STDMETHODIMP CreateTask(/* in */ DWORD dwStackSize, /* in */ LPTHREAD_START_ROUTINE pStartAddress, /* in */ PVOID pParameter, /* out */ IHostTask **ppTask);
   STDMETHODIMP Sleep(/* in */ DWORD dwMilliseconds, /* in */ DWORD option);
   STDMETHODIMP SwitchToTask(/* in */ DWORD option);
   STDMETHODIMP SetUILocale(/* in */ LCID lcid);
   STDMETHODIMP SetLocale(/* in */ LCID lcid);
   STDMETHODIMP CallNeedsHostHook(/* in */ SIZE_T target, /* out */ BOOL *pbCallNeedsHostHook);
   STDMETHODIMP LeaveRuntime(/* in */ SIZE_T target);
   STDMETHODIMP EnterRuntime();
   STDMETHODIMP ReverseEnterRuntime();
   STDMETHODIMP ReverseLeaveRuntime();
   STDMETHODIMP BeginDelayAbort();
   STDMETHODIMP EndDelayAbort();
   STDMETHODIMP BeginThreadAffinity();
   STDMETHODIMP EndThreadAffinity();
   STDMETHODIMP SetStackGuarantee(/* in */ ULONG guarantee);
   STDMETHODIMP GetStackGuarantee(/* out */ ULONG *pGuarantee);
   STDMETHODIMP SetCLRTaskManager(/* in */ ICLRTaskManager *pManager);

   ICLRTaskManager* GetCLRTaskManager() { return m_pCLRTaskManager; }
   void AddManagedTask(IHostTask* hostTask, ICLRTask* managedTask, DWORD nativeThreadId);
   void RemoveTask(DWORD nativeThreadId);
   bool IsSnippetThread(DWORD nativeThreadId);

};

#endif //TASK_MANAGER_H_INCLUDED
