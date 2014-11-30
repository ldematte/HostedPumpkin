#include "TaskMgr.h"
#include "Task.h"

#include "../CrstLock.h"
#include "../Logger.h"

SHTaskManager::SHTaskManager(DHContext *pContext) {
   m_cRef = 0;
   m_pCLRTaskManager = NULL;
   m_pContext = pContext;
   m_hSleepEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

   if (!m_hSleepEvent) {
      Logger::Critical("Failed to create a new sleep event");
   }

   // Instantiate maps and associated critical sections.
   m_pThreadMap = new std::map < DWORD, IHostTask* > ;
   m_pThreadMapCrst = new CRITICAL_SECTION;

   if (!m_pThreadMap || !m_pThreadMapCrst)
      Logger::Critical("Failed to allocate new map or CRST");

   InitializeCriticalSection(m_pThreadMapCrst);
}

SHTaskManager::~SHTaskManager() {
   if (m_hSleepEvent) CloseHandle(m_hSleepEvent);
   if (m_pThreadMap) delete m_pThreadMap;
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

STDMETHODIMP SHTaskManager::GetCurrentTask(/* out */ IHostTask **pTask) {
   DWORD currentThreadId = GetCurrentThreadId();

   CrstLock crst(m_pThreadMapCrst);
   std::map<DWORD, IHostTask*>::iterator match = m_pThreadMap->find(currentThreadId);
   if (match == m_pThreadMap->end()) {
      // No match was found, create one for the currently executing thread.
      *pTask = new SHTask(this, GetCurrentThread());
      m_pThreadMap->insert(std::map<DWORD, IHostTask*>::value_type(currentThreadId, *pTask));
   }
   else {
      *pTask = match->second;
   }
   (*pTask)->AddRef();
   crst.Exit();

   return S_OK;
}

STDMETHODIMP SHTaskManager::CreateTask(/* in */ DWORD dwStackSize, /* in */ LPTHREAD_START_ROUTINE pStartAddress, /* in */ PVOID pParameter, /* out */ IHostTask **ppTask) {
   DWORD dwThreadId;
   HANDLE hThread = CreateThread(
      NULL,
      dwStackSize,
      pStartAddress,
      pParameter,
      CREATE_SUSPENDED | STACK_SIZE_PARAM_IS_A_RESERVATION,
      &dwThreadId);

   IHostTask* task = new SHTask(this, hThread);
   if (!task) {
      _ASSERTE(!"Failed to allocate task");
      *ppTask = NULL;
      return E_OUTOFMEMORY;
   }

   CrstLock crst(m_pThreadMapCrst);
   m_pThreadMap->insert(std::map<DWORD, IHostTask*>::value_type(dwThreadId, task));
   crst.Exit();

   task->AddRef();
   *ppTask = task;

   return S_OK;
}

STDMETHODIMP SHTaskManager::Sleep(/* in */ DWORD dwMilliseconds, /* in */ DWORD option) {
   // TODO: recognize 'option'?
   ::SleepEx(dwMilliseconds, option & WAIT_ALERTABLE);
   return S_OK;
}

STDMETHODIMP SHTaskManager::SwitchToTask(/* in */ DWORD option) {
   //TODO: recognize 'option'?
   SwitchToThread();
   return S_OK;
}

STDMETHODIMP SHTaskManager::SetUILocale(/* in */ LCID lcid) {
   _ASSERTE(!"Not implemented");
   return E_NOTIMPL;
}

STDMETHODIMP SHTaskManager::SetLocale(/* in */ LCID lcid) {
   if (!SetThreadLocale(lcid)) {
      _ASSERTE(!"Couldn't set thread-locale");
      return HRESULT_FROM_WIN32(GetLastError());
   }

   return S_OK;
}

STDMETHODIMP SHTaskManager::CallNeedsHostHook(/* in */ SIZE_T target, /* out */ BOOL *pbCallNeedsHostHook) {
   *pbCallNeedsHostHook = FALSE;
   return S_OK;
}

STDMETHODIMP SHTaskManager::LeaveRuntime(/* in */ SIZE_T target) {
   // No need to perform any processing.
   return S_OK;
}

STDMETHODIMP SHTaskManager::EnterRuntime() {
   // No need to perform any processing.
   return S_OK;
}

STDMETHODIMP SHTaskManager::ReverseLeaveRuntime() {
   // No need to perform any processing.
   return S_OK;
}

STDMETHODIMP SHTaskManager::ReverseEnterRuntime() {
   // No need to perform any processing.
   return S_OK;
}

STDMETHODIMP SHTaskManager::BeginDelayAbort() {
   // We don't use aborts in this host; no-op.
   return S_OK;
}

STDMETHODIMP SHTaskManager::EndDelayAbort() {
   // We don't use aborts in this host; no-op.
   return S_OK;
}

STDMETHODIMP SHTaskManager::BeginThreadAffinity() {
   // We don't move tasks in this host; no-op.
   return S_OK;
}

STDMETHODIMP SHTaskManager::EndThreadAffinity() {
   // We don't move tasks in this host; no-op.
   return S_OK;
}

STDMETHODIMP SHTaskManager::SetStackGuarantee(/* in */ ULONG guarantee) {
   _ASSERTE(!"Not implemented");
   return E_NOTIMPL;
}

STDMETHODIMP SHTaskManager::GetStackGuarantee(/* out */ ULONG *pGuarantee) {
   _ASSERTE(!"Not implemented");
   return E_NOTIMPL;
}

STDMETHODIMP SHTaskManager::SetCLRTaskManager(/* in */ ICLRTaskManager *pManager) {
   m_pCLRTaskManager = pManager;
   return S_OK;
}
