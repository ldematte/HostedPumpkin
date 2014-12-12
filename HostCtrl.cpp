
#include <metahost.h>

#include "HostCtrl.h"
#include "Threading\TaskMgr.h"
#include "Threading\SyncMgr.h"
#include "Memory\MemoryMgr.h"
#include "Memory\GCMgr.h"
#include "Threading\ThreadpoolMgr.h"
#include "Threading\IoCompletionMgr.h"

#include "Logger.h"

DHHostControl::DHHostControl(ICLRRuntimeHost *pRuntimeHost) {
   m_cRef = 0;
   m_pRuntimeHost = pRuntimeHost;
   m_pRuntimeHost->AddRef();

   if (!SUCCEEDED(pRuntimeHost->GetCLRControl(&m_pRuntimeControl))) {
      Logger::Critical("Failed to obtain a CLR Control object");
   }

   taskManager = new SHTaskManager();
   syncManager = new SHSyncManager();
   memoryManager = new SHMemoryManager();
   gcManager = new SHGCManager();
   threadpoolManager = new SHThreadpoolManager();
   iocpManager = new SHIoCompletionManager();


   if (!taskManager || !syncManager ||
      !memoryManager || !gcManager || !threadpoolManager || !iocpManager) {
      Logger::Critical("Unable to allocate Host Managers");
   }

   taskManager->AddRef();
   syncManager->AddRef();
   memoryManager->AddRef();
   gcManager->AddRef();
   threadpoolManager->AddRef();
   iocpManager->AddRef();
}

DHHostControl::~DHHostControl() {
   m_pRuntimeHost->Release();
   m_pRuntimeControl->Release();
   taskManager->Release();
   syncManager->Release();
   memoryManager->Release();
   gcManager->Release();
   threadpoolManager->Release();
   iocpManager->Release();
}

STDMETHODIMP_(VOID) DHHostControl::ShuttingDown() {
}

// IUnknown functions

STDMETHODIMP_(DWORD) DHHostControl::AddRef() {
   return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(DWORD) DHHostControl::Release() {
   ULONG cRef = InterlockedDecrement(&m_cRef);
   if (cRef == 0)
      delete this;
   return cRef;
}

STDMETHODIMP DHHostControl::QueryInterface(const IID &riid, void **ppvObject) {
   if (riid == IID_IUnknown || riid == IID_IHostControl) {
      *ppvObject = this;
      AddRef();
      return S_OK;
   }

   *ppvObject = NULL;
   return E_NOINTERFACE;
}

// IHostControl functions

STDMETHODIMP DHHostControl::GetHostManager(const IID &riid, void **ppvHostManager) {
   if (riid == IID_IHostTaskManager)
      taskManager->QueryInterface(IID_IHostTaskManager, ppvHostManager);
   else if (riid == IID_IHostSyncManager)
      syncManager->QueryInterface(IID_IHostSyncManager, ppvHostManager);
   else if (riid == IID_IHostMemoryManager)
      memoryManager->QueryInterface(IID_IHostMemoryManager, ppvHostManager);
   else if (riid == IID_IHostGCManager)
      gcManager->QueryInterface(IID_IHostGCManager, ppvHostManager);
   else if (riid == IID_IHostIoCompletionManager)
      iocpManager->QueryInterface(IID_IHostIoCompletionManager, ppvHostManager);
   else if (riid == IID_IHostThreadpoolManager)
      threadpoolManager->QueryInterface(IID_IHostThreadpoolManager, ppvHostManager);
   else
      ppvHostManager = NULL;

   return S_OK;
}

STDMETHODIMP DHHostControl::SetAppDomainManager(DWORD dwAppDomainID, IUnknown *pUnkAppDomainManager) {
   Logger::Info("In HostControl::SetAppDomainManager");

   ISimpleHostDomainManager* domainManager = NULL;

   ICLRTask* currentTask;
   taskManager->GetCLRTaskManager()->GetCurrentTask(&currentTask);
   Logger::Debug("New AppDomain %d. Current task is %x - (on thread %d)", dwAppDomainID, currentTask, ::GetCurrentThreadId());
   //[out] A pointer to the address of an ICLRTask instance that is currently executing on the operating system thread 
   //from which the call originated, or null if no task is currently executing on this thread.

   // TODO: this may not be the ideal place; it depends on how the domains are created.
   // taskManager->RegisterNewAppDomain(dwAppDomainID, currentTask, ::GetCurrentThreadId());

   HRESULT hr = pUnkAppDomainManager->QueryInterface(__uuidof(ISimpleHostDomainManager), (PVOID*) &domainManager);

   if (SUCCEEDED(hr)) {
      hostContext.appDomains.insert(std::make_pair(dwAppDomainID, domainManager));
      if (hostContext.defaultDomainManager == NULL)
         hostContext.defaultDomainManager = domainManager;
   }
   else {
      hostContext.appDomains[dwAppDomainID] = NULL;
   }

   return hr;
}

ISimpleHostDomainManager* DHHostControl::GetDomainManagerForDefaultDomain() {
   if (hostContext.defaultDomainManager)
      hostContext.defaultDomainManager->AddRef();

   return hostContext.defaultDomainManager;
}

