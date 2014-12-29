#include "Task.h"

#include "../HostContext.h"
#include "../Logger.h"

const int INVALID_THREAD_ID = 0;

// Standard functions

SHTask::SHTask(SHTaskManager *pTaskManager, DWORD nativeThreadId, HANDLE hThread) {
   m_cRef = 0;
   m_pTaskManager = pTaskManager;
   m_pTaskManager->AddRef();
   m_nativeId = nativeThreadId;
   m_hThread = hThread;
   m_pCLRTask = NULL;
}

// For the current thread
SHTask::SHTask(SHTaskManager *pTaskManager, DWORD nativeThreadId) {
   m_cRef = 0;
   m_pTaskManager = pTaskManager;
   m_pTaskManager->AddRef();
   m_nativeId = nativeThreadId;
   // Get a real handle from the "current thread" pseudo-handle
   DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &m_hThread, 0, FALSE, DUPLICATE_SAME_ACCESS);
   m_pCLRTask = NULL;
}

SHTask::~SHTask() {
   if (m_nativeId != INVALID_THREAD_ID) {
      m_pTaskManager->RemoveTask(m_nativeId);
   }
   
   //TODO: shutdown thread?

   if (m_hThread != INVALID_HANDLE_VALUE) {
      CloseHandle(m_hThread);
   }

   if (m_pTaskManager) m_pTaskManager->Release();
   if (m_pCLRTask) m_pCLRTask->Release();
}

// IUnknown functions

STDMETHODIMP_(DWORD) SHTask::AddRef() {
   return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(DWORD) SHTask::Release() {
   ULONG cRef = InterlockedDecrement(&m_cRef);
   if (cRef == 0)
      delete this;
   return cRef;
}

STDMETHODIMP SHTask::QueryInterface(const IID &riid, void **ppvObject) {
   if (riid == IID_IUnknown || riid == IID_IHostTask) {
      *ppvObject = this;
      AddRef();
      return S_OK;
   }

   *ppvObject = NULL;
   return E_NOINTERFACE;
}

// IHostTask functions

STDMETHODIMP SHTask::Start() {
   Logger::Info("In Task::Start");
   if (!ResumeThread(m_hThread)) {
      Logger::Error("Couldn't resume thread");
      return HRESULT_FROM_WIN32(GetLastError());
   }

   return S_OK;
}

VOID WINAPI APCFunc(ULONG_PTR) {
   //Nothing to do in here
}

STDMETHODIMP SHTask::Alert() {
   Logger::Info("In Task::Alert");
   QueueUserAPC(APCFunc, m_hThread, NULL);
   return S_OK;
}

STDMETHODIMP SHTask::Join(/* in */ DWORD dwMilliseconds, /* in */ DWORD dwOption) {
   Logger::Info("In Task::Join %d milliseconds, %d options", dwMilliseconds, dwOption);
   return HostContext::HostWait(m_hThread, dwMilliseconds, dwOption);
}

STDMETHODIMP SHTask::SetPriority(/* in */ int newPriority) {
   Logger::Debug("In Task::SetPriority %d", newPriority);

   // Do not allow managed threads (any of them) to increase their priority
   // From MSDN: "A host can define its own algorithms for thread priority assignment, and is free to ignore this request."
   if (newPriority > THREAD_PRIORITY_NORMAL && m_pTaskManager->IsSnippetThread(m_nativeId))
      return S_OK;

   if (!SetThreadPriority(m_hThread, newPriority)) {
      Logger::Error("Couldn't set thread-priority");
      return HRESULT_FROM_WIN32(GetLastError());
   }

   return S_OK;
}

STDMETHODIMP SHTask::GetPriority(/* out */ int *pPriority) {
   Logger::Info("In Task::GetPriority");
   *pPriority = GetThreadPriority(m_hThread);
   return S_OK;
}

STDMETHODIMP SHTask::SetCLRTask(/* in */ ICLRTask *pCLRTask) {
   Logger::Debug("In Task::SetCLRTask for %d -- clr: %x, host: %x", m_nativeId, pCLRTask, this);
   m_pTaskManager->AddManagedTask(this, pCLRTask, m_nativeId);
   m_pCLRTask = pCLRTask;
   return S_OK;
}
