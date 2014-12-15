#include "TaskMgr.h"
#include "Task.h"

#include "../CrstLock.h"
#include "../Logger.h"
#include "../HostContext.h"

//unreferenced formal parameter
#pragma warning (disable: 4100)

SHTaskManager::SHTaskManager() {
   m_cRef = 0;
   m_pCLRTaskManager = NULL;
   
   // Instantiate maps and associated critical sections.
   nativeThreadMapCrst = new CRITICAL_SECTION;
   managedThreadMapCrst = new CRITICAL_SECTION;

   if (!nativeThreadMapCrst || !managedThreadMapCrst)
      Logger::Critical("Failed to allocate critical sections");

   InitializeCriticalSection(nativeThreadMapCrst);
   InitializeCriticalSection(managedThreadMapCrst);
}

SHTaskManager::~SHTaskManager() {
   if (nativeThreadMapCrst) DeleteCriticalSection(nativeThreadMapCrst);
   if (managedThreadMapCrst) DeleteCriticalSection(managedThreadMapCrst);
   if (m_pCLRTaskManager) m_pCLRTaskManager->Release();
}

// IUnknown functions

STDMETHODIMP_(DWORD) SHTaskManager::AddRef() {
   return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(DWORD) SHTaskManager::Release() {
   ULONG cRef = InterlockedDecrement(&m_cRef);
   if (cRef == 0)
      delete this;
   return cRef;
}

STDMETHODIMP SHTaskManager::QueryInterface(const IID &riid, void **ppvObject) {
   if (riid == IID_IUnknown || riid == IID_IHostTaskManager) {
      *ppvObject = this;
      AddRef();
      return S_OK;
   }

   *ppvObject = NULL;
   return E_NOINTERFACE;
}

// IHostTaskManager functions
STDMETHODIMP SHTaskManager::GetCurrentTask(/* out */ IHostTask **pTask) {
   Logger::Info("TaskManager::GetCurrentTask");
   DWORD currentThreadId = GetCurrentThreadId();

   CrstLock crst(nativeThreadMapCrst);
   std::map<DWORD, IHostTask*>::iterator match = nativeThreadMap.find(currentThreadId);
   if (match == nativeThreadMap.end()) {
      // No match was found, create one for the currently executing thread.      
      *pTask = new SHTask(this, currentThreadId);
      Logger::Debug("Created task for EXISTING thread %d - %x", currentThreadId, pTask);
      nativeThreadMap.insert(std::map<DWORD, IHostTask*>::value_type(currentThreadId, *pTask));
   }
   else {
      *pTask = match->second;
   }
   (*pTask)->AddRef();
   crst.Exit();

   return S_OK;
}

STDMETHODIMP SHTaskManager::CreateTask(/* in */ DWORD dwStackSize, /* in */ LPTHREAD_START_ROUTINE pStartAddress, /* in */ PVOID pParameter, /* out */ IHostTask **ppTask) {
   Logger::Info("TaskManager::CreateTask");
   DWORD dwThreadId;
   HANDLE hThread = CreateThread(
      NULL,
      dwStackSize,
      pStartAddress,
      pParameter,
      CREATE_SUSPENDED | STACK_SIZE_PARAM_IS_A_RESERVATION,
      &dwThreadId);

   IHostTask* task = new SHTask(this, dwThreadId, hThread);
   if (!task) {
      Logger::Error("Failed to allocate task");
      *ppTask = NULL;
      return E_OUTOFMEMORY;
   }
   Logger::Debug("Created task for NEW thread %d - %x -- child of %d", dwThreadId, task, ::GetCurrentThreadId());

   CrstLock crst(nativeThreadMapCrst);
   nativeThreadMap.insert(std::map<DWORD, IHostTask*>::value_type(dwThreadId, task));
   crst.Exit();

   task->AddRef();
   *ppTask = task;

   return S_OK;
}

STDMETHODIMP SHTaskManager::Sleep(/* in */ DWORD dwMilliseconds, /* in */ DWORD option) {
   Logger::Info("TaskManager::Sleep");
   return HostContext::Sleep(dwMilliseconds, option);
}

STDMETHODIMP SHTaskManager::SwitchToTask(/* in */ DWORD option) {
   Logger::Info("TaskManager::SwitchToTask");
   //TODO: recognize 'option'?
   SwitchToThread();
   return S_OK;
}

STDMETHODIMP SHTaskManager::SetUILocale(/* in */ LCID lcid) {
   Logger::Error("TaskManager::SetUILocale: Not implemented");
   return E_NOTIMPL;
}

STDMETHODIMP SHTaskManager::SetLocale(/* in */ LCID lcid) {
   Logger::Info("TaskManager::SetLocale");

   if (!SetThreadLocale(lcid)) {
      Logger::Error("Couldn't set thread-locale");
      return HRESULT_FROM_WIN32(GetLastError());
   }

   return S_OK;
}

STDMETHODIMP SHTaskManager::CallNeedsHostHook(/* in */ SIZE_T target, /* out */ BOOL *pbCallNeedsHostHook) {
   Logger::Info("TaskManager::CallNeedsHostHook");
   // Do not inline P/Invoke calls
   *pbCallNeedsHostHook = FALSE;
   return S_OK;
}

STDMETHODIMP SHTaskManager::LeaveRuntime(/* in */ SIZE_T target) {
   Logger::Info("TaskManager::LeaveRuntime");
   // No need to perform any processing.
   return S_OK;
}

STDMETHODIMP SHTaskManager::EnterRuntime() {
   Logger::Info("TaskManager::EnterRuntime");
   // No need to perform any processing.
   return S_OK;
}

STDMETHODIMP SHTaskManager::ReverseLeaveRuntime() {
   Logger::Info("TaskManager::ReverseLeaveRuntime");
   // No need to perform any processing.
   return S_OK;
}

STDMETHODIMP SHTaskManager::ReverseEnterRuntime() {
   Logger::Info("TaskManager::ReverseEnterRuntime");
   // No need to perform any processing.
   return S_OK;
}

STDMETHODIMP SHTaskManager::BeginDelayAbort() {
   Logger::Info("TaskManager::BeginDelayAbort");
   // We don't use aborts in this host; no-op.
   return S_OK;
}

STDMETHODIMP SHTaskManager::EndDelayAbort() {
   Logger::Info("TaskManager::EndDelayAbort");
   // We don't use aborts in this host; no-op.
   return S_OK;
}

STDMETHODIMP SHTaskManager::BeginThreadAffinity() {
   Logger::Info("TaskManager::BeginThreadAffinity");
   // We don't move tasks in this host; no-op.
   return S_OK;
}

STDMETHODIMP SHTaskManager::EndThreadAffinity() {
   Logger::Info("TaskManager::EndThreadAffinity");
   // We don't move tasks in this host; no-op.
   return S_OK;
}

STDMETHODIMP SHTaskManager::SetStackGuarantee(/* in */ ULONG guarantee) {
   //http://msdn.microsoft.com/en-us/library/aa964918%28v=vs.110%29.aspx
   // "Reserved for internal use only"
   Logger::Info("TaskManager::SetStackGuarantee Not implemented");
   return E_NOTIMPL;
}

STDMETHODIMP SHTaskManager::GetStackGuarantee(/* out */ ULONG *pGuarantee) {
   //http://msdn.microsoft.com/en-us/library/aa964918%28v=vs.110%29.aspx
   // "Reserved for internal use only"
   Logger::Info("TaskManager::SetStackGuarantee Not implemented");
   return E_NOTIMPL;
}

STDMETHODIMP SHTaskManager::SetCLRTaskManager(/* in */ ICLRTaskManager *pManager) {
   m_pCLRTaskManager = pManager;
   return S_OK;
}

// Estra bookeeping functions
void SHTaskManager::AddManagedTask(IHostTask* hostTask, ICLRTask* managedTask, DWORD nativeThreadId) {
#ifdef _DEBUG
   {
      CrstLock lock(nativeThreadMapCrst);
      auto iter = nativeThreadMap.find(nativeThreadId);
      if (iter == nativeThreadMap.end()) {
         Logger::Error("Cannot find Native task %d (%x)", nativeThreadId, nativeThreadId);
      }
      else {
         if (iter->second != hostTask) {
            Logger::Critical("Native task for %d mismatch! (%x - %x)", nativeThreadId, iter->second, hostTask);
         }
      }
   }
#endif
   managedTask->AddRef();
   CrstLock managedMapLock(managedThreadMapCrst);
   managedThreadMap.insert(std::make_pair(nativeThreadId, managedTask));
   managedMapLock.Exit();
}

void SHTaskManager::RemoveTask(DWORD nativeThreadId) {
   Logger::Debug("In TaskManager::RemoveTask: %d", nativeThreadId);

   CrstLock nativeMapLock(nativeThreadMapCrst);
   nativeThreadMap.erase(nativeThreadId);
   // TODO: from other locations as well!   
   nativeMapLock.Exit();
   
   CrstLock managedMapLock(managedThreadMapCrst);
   auto managedTask = managedThreadMap.find(nativeThreadId);
   if (managedTask != managedThreadMap.end()) {
      // TODO: call ExitTask too?
      managedTask->second->Release();
      managedThreadMap.erase(managedTask);
   }
   managedMapLock.Exit();
}
