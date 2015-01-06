
#include <metahost.h>

#include "HostCtrl.h"
#include "Threading\TaskMgr.h"
#include "Threading\CLRThread.h"
#include "Threading\SyncMgr.h"
#include "Memory\MemoryMgr.h"
#include "Memory\GCMgr.h"
#include "Threading\ThreadpoolMgr.h"
#include "Threading\IoCompletionMgr.h"
#include "Assembly\AssemblyMgr.h"
#include "EventManager.h"
#include "PolicyManager.h"

#include "Logger.h"

DHHostControl::DHHostControl(ICLRRuntimeHost *pRuntimeHost, const std::list<AssemblyInfo>& hostAssemblies) {
   m_cRef = 0;
   m_pRuntimeHost = pRuntimeHost;
   m_pRuntimeHost->AddRef();

   if (!SUCCEEDED(pRuntimeHost->GetCLRControl(&m_pRuntimeControl))) {
      Logger::Critical("Failed to obtain a CLR Control object");
   }

   hostContext = new HostContext(m_pRuntimeHost);
   if (!hostContext) {
      Logger::Critical("Unable to allocate Host Context");
   }  

   taskManager = new SHTaskManager(hostContext);
   syncManager = new SHSyncManager();
   memoryManager = new SHMemoryManager(hostContext);
   gcManager = new SHGCManager();
   threadpoolManager = new SHThreadpoolManager(hostContext);
   iocpManager = new SHIoCompletionManager(hostContext);
   assemblyManager = new SHAssemblyManager(hostAssemblies);
   eventManager = new SHEventManager(hostContext);
   policyManager = new SHPolicyManager(hostContext);

   // Register our Event Manager for the events we are interested in
   ICLROnEventManager* clrEventManager;
   if (SUCCEEDED(m_pRuntimeControl->GetCLRManager(IID_ICLROnEventManager, (void**) &clrEventManager))) {
      clrEventManager->RegisterActionOnEvent(Event_DomainUnload, eventManager);
      clrEventManager->RegisterActionOnEvent(Event_ClrDisabled, eventManager);
      clrEventManager->RegisterActionOnEvent(Event_MDAFired, eventManager);
   }


   if (!taskManager || !syncManager || !memoryManager || !gcManager || !threadpoolManager || 
      !iocpManager || !assemblyManager || !eventManager || !policyManager) {
      Logger::Critical("Unable to allocate Host Managers");
   }

   hostContext->AddRef();

   taskManager->AddRef();
   syncManager->AddRef();
   memoryManager->AddRef();
   gcManager->AddRef();
   threadpoolManager->AddRef();
   iocpManager->AddRef();
   assemblyManager->AddRef();
   eventManager->AddRef();
   policyManager->AddRef();

   SetupEscalationPolicy();
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
   assemblyManager->Release();
   eventManager->Release();
   policyManager->Release();

   hostContext->Release();
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
   if (ppvObject == NULL)
      return E_INVALIDARG;

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
      return taskManager->QueryInterface(IID_IHostTaskManager, ppvHostManager);
   if (riid == IID_IHostSyncManager)
      return syncManager->QueryInterface(IID_IHostSyncManager, ppvHostManager);
   if (riid == IID_IHostMemoryManager)
      return memoryManager->QueryInterface(IID_IHostMemoryManager, ppvHostManager);
   if (riid == IID_IHostGCManager)
      return gcManager->QueryInterface(IID_IHostGCManager, ppvHostManager);
   if (riid == IID_IHostIoCompletionManager)
      return iocpManager->QueryInterface(IID_IHostIoCompletionManager, ppvHostManager);
   if (riid == IID_IHostThreadpoolManager)
      return threadpoolManager->QueryInterface(IID_IHostThreadpoolManager, ppvHostManager);
   if (riid == IID_IHostAssemblyManager)
      return assemblyManager->QueryInterface(IID_IHostAssemblyManager, ppvHostManager);
   if (riid == IID_IActionOnCLREvent)
      return eventManager->QueryInterface(IID_IActionOnCLREvent, ppvHostManager);
   if (riid == IID_IHostPolicyManager)
      return policyManager->QueryInterface(IID_IHostPolicyManager, ppvHostManager);
   
   ppvHostManager = NULL;
   return E_NOINTERFACE;
}

STDMETHODIMP DHHostControl::SetAppDomainManager(DWORD dwAppDomainID, IUnknown *pUnkAppDomainManager) {
   Logger::Info("In HostControl::SetAppDomainManager");

   ISimpleHostDomainManager* domainManager = NULL;

   ICLRTask* currentTask;
   taskManager->GetCLRTaskManager()->GetCurrentTask(&currentTask);
   Logger::Debug("New AppDomain %d. Current task is %x - (on thread %d)", dwAppDomainID, currentTask, ::GetCurrentThreadId());
   //[out] A pointer to the address of an ICLRTask instance that is currently executing on the operating system thread 
   //from which the call originated, or null if no task is currently executing on this thread.

   HRESULT hr = pUnkAppDomainManager->QueryInterface(__uuidof(ISimpleHostDomainManager), (PVOID*) &domainManager);

   if (FAILED(hr)) {
      domainManager = NULL;
   }
   

   if (domainManager) {
      // Call ISimpleHostDomainManager->GetMainThreadManagedId to perform a cross-check
      Logger::Debug("New AppDomain %d has main thread (managed) %d", dwAppDomainID, domainManager->GetMainThreadManagedId());
      // Perform cross-check with the Thread*
      CLRThread* thread = (CLRThread*)currentTask;
      Logger::Debug("Main thread (managed) info. Id: %d", thread->m_ThreadId);
   }
   

   // TODO: this may not be the ideal place; it depends on how the domains are created.
   // taskManager->RegisterNewAppDomain(dwAppDomainID, currentTask, ::GetCurrentThreadId());
   hostContext->OnDomainCreate(dwAppDomainID, ::GetCurrentThreadId(), domainManager);
   return hr;
}

ISimpleHostDomainManager* DHHostControl::GetDomainManagerForDefaultDomain() {
   return hostContext->GetDomainManagerForDefaultDomain();   
}

IHostContext* DHHostControl::GetHostContext() {
   //if (hostContext)
   //   hostContext->AddRef();
   return hostContext;
}

bool DHHostControl::SetupEscalationPolicy() {
   ICLRPolicyManager* clrPolicyManager = NULL;

   HRESULT hr = m_pRuntimeControl->GetCLRManager(IID_ICLRPolicyManager, (void**)&clrPolicyManager);

   if (SUCCEEDED(hr)) {

      /////////////////////
      // Action -> Failures

      // If we are not able to obtain a resource (OutOfMemoryException), 
      // ALTERNATIVE 1: unload the AppDomain (eUnloadAppDomain)
      // ALTERNATIVE 2: abort the calling thread (eAbortThread)
      hr = clrPolicyManager->SetActionOnFailure(FAIL_NonCriticalResource, eThrowException);
      if (FAILED(hr)) {
         Logger::Critical("Cannot SetActionOnFailure: HRESULT %x", hr);
         return false;
      }
      hr = clrPolicyManager->SetActionOnFailure(FAIL_CriticalResource, eUnloadAppDomain);
      if (FAILED(hr)) {
         Logger::Critical("Cannot SetActionOnFailure: HRESULT %x", hr);
         return false;
      }
      //Not supported in the.NET Framework 4
      //hr = clrPolicyManager->SetActionOnFailure(FAIL_AccessViolation, eUnloadAppDomain);
      //if (FAILED(hr)) {
      //   Logger::Critical("Cannot SetActionOnFailure: HRESULT %x", hr);
      //   return false;
      //}

      // Same if we get a orphaned lock: we only allow the usage of "AppDomain local" threading
      // primitives, so if it is orphaned, it is leaked only locally. "Solve" this by unloading
      // the AppDomain
      // TODO: disallow OR rename named synchronization promitives!
      hr = clrPolicyManager->SetActionOnFailure(FAIL_OrphanedLock, eUnloadAppDomain);
      if (FAILED(hr)) {
         Logger::Critical("Cannot SetActionOnFailure: HRESULT %x", hr);
         return false;
      }

      // On a fatal runtime error, recycle the process
      hr = clrPolicyManager->SetActionOnFailure(FAIL_FatalRuntime, eRudeExitProcess); // OR eDisableRuntime
      if (FAILED(hr)) {
         Logger::Critical("Cannot SetActionOnFailure: HRESULT %x", hr);
         return false;
      }

      // On StackOverflow, the best we can do is a rude AppDomain unload. This will increase our
      // "zombie" counter but it's better than abruptly tearing down evrerything
      hr = clrPolicyManager->SetActionOnFailure(FAIL_StackOverflow, eRudeUnloadAppDomain);
      if (FAILED(hr)) {
         Logger::Critical("Cannot SetActionOnFailure: HRESULT %x", hr);
         return false;
      }


      /////////////////////
      // Timeouts -> Escalation

      hr = clrPolicyManager->SetTimeoutAndAction(OPR_ThreadAbort, THREAD_ABORT_TIMEOUT * 1000, eRudeAbortThread);
      if (FAILED(hr)) {
         Logger::Critical("Cannot SetTimeoutAndAction: HRESULT %x", hr);
         return false;
      }    
      hr = clrPolicyManager->SetTimeout(OPR_FinalizerRun, THREAD_ABORT_TIMEOUT/2 * 1000);
      if (FAILED(hr)) {
         Logger::Critical("Cannot SetTimeoutAndAction: HRESULT %x", hr);
         return false;
      }
      hr = clrPolicyManager->SetTimeoutAndAction(OPR_AppDomainUnload, APPDOMAIN_UNLOAD_TIMEOUT * 1000, eRudeUnloadAppDomain);
      if (FAILED(hr)) {
         Logger::Critical("Cannot SetTimeoutAndAction: HRESULT %x", hr);
         return false;
      }

      ////////////////////
      // Default actions (escalation without timeout)

      hr = clrPolicyManager->SetDefaultAction(OPR_ThreadRudeAbortInCriticalRegion, eRudeUnloadAppDomain);
      if (FAILED(hr)) {
         Logger::Critical("Cannot SetDefaultAction: HRESULT %x", hr);
         return false;
      }

      // And specify what we should do on unhandled exceptions
      hr = clrPolicyManager->SetUnhandledExceptionPolicy(eHostDeterminedPolicy);
      if (FAILED(hr)) {
         Logger::Critical("Cannot SetUnhandledExceptionPolicy: HRESULT %x", hr);
         return false;
      }

      clrPolicyManager->Release();
   }
   return true;
}

