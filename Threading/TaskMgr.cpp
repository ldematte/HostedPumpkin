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
   m_hSleepEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

   if (!m_hSleepEvent) {
      Logger::Critical("Failed to create a new sleep event");
   }

   // Instantiate maps and associated critical sections.
   m_pThreadMapCrst = new CRITICAL_SECTION;

   if (!m_pThreadMapCrst)
      Logger::Critical("Failed to allocate new CRST");

   InitializeCriticalSection(m_pThreadMapCrst);
}

SHTaskManager::~SHTaskManager() {
   if (m_hSleepEvent) CloseHandle(m_hSleepEvent);
   if (m_pThreadMapCrst) DeleteCriticalSection(m_pThreadMapCrst);
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
void SHTaskManager::AddManagedTask(ICLRTask* managedTask, DWORD nativeThreadId) {
    managedThreadMap.insert(std::make_pair(managedTask, nativeThreadId));
}

STDMETHODIMP SHTaskManager::GetCurrentTask(/* out */ IHostTask **pTask) {
   Logger::Info("TaskManager::GetCurrentTask");
   DWORD currentThreadId = GetCurrentThreadId();

   CrstLock crst(m_pThreadMapCrst);
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

   CrstLock crst(m_pThreadMapCrst);
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
