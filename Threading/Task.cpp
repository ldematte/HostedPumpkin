#include "Task.h"

#include "../HostContext.h"
#include "../Logger.h"

// Standard functions

SHTask::SHTask(SHTaskManager *pTaskManager, HANDLE hThread) {
   m_cRef = 0;
   m_pTaskManager = pTaskManager;
   m_pTaskManager->AddRef();
   m_hThread = hThread;
   m_pCLRTask = NULL;
}

SHTask::~SHTask() {
   //TODO: shutdown thread?
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

STDMETHODIMP SHTask::Alert() {
   Logger::Info("In Task::Alert");
   return S_OK;
}

STDMETHODIMP SHTask::Join(/* in */ DWORD dwMilliseconds, /* in */ DWORD dwOption) {
   Logger::Info("In Task::Join %d milliseconds, %d options", dwMilliseconds, dwOption);
   return HostContext::HostWait(m_hThread, dwMilliseconds, dwOption);
}

STDMETHODIMP SHTask::SetPriority(/* in */ int newPriority) {
   Logger::Info("In Task::SetPriority %d", newPriority);
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
   Logger::Info("In Task::SetCLRTask");
   m_pCLRTask = pCLRTask;
   return S_OK;
}
