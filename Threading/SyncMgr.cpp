#include "SyncMgr.h"

#include "../Context.h"
#include "../Logger.h"

// Include all the memory objects created by this manager
#include "Crst.h"
#include "AutoEvent.h"
#include "ManualEvent.h"
#include "Semaphore.h"

SHSyncManager::SHSyncManager(DHContext *pContext) {
   m_cRef = 0;
   m_pCLRSyncManager = NULL;
   m_pContext = pContext;
}

SHSyncManager::~SHSyncManager() {
   m_pCLRSyncManager->Release();
}

// IUnknown functions

STDMETHODIMP_(DWORD) SHSyncManager::AddRef() {
   return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(DWORD) SHSyncManager::Release() {
   ULONG cRef = InterlockedDecrement(&m_cRef);
   if (cRef == 0) {
      delete this;
   }
   return cRef;
}

STDMETHODIMP SHSyncManager::QueryInterface(const IID &riid, void **ppvObject) {
   if (riid == IID_IUnknown || riid == IID_IHostSyncManager) {
      *ppvObject = this;
      AddRef();
      return S_OK;
   }

   *ppvObject = NULL;
   return E_NOINTERFACE;
}

// IHostSyncManager functions

STDMETHODIMP SHSyncManager::SetCLRSyncManager(/* in */ ICLRSyncManager *pManager) {
   Logger::Info("In SyncManager::SetCLRSyncManager");
   m_pCLRSyncManager = pManager;
   return S_OK;
}

STDMETHODIMP SHSyncManager::CreateCrst(/* out */ IHostCrst **ppCrst) {
   Logger::Info("In SyncManager::CreateCrst");

   IHostCrst* pCrst = new SHCrst;
   if (!pCrst) {
      Logger::Error("Failed to allocate a new Crst");
      *ppCrst = NULL;
      return E_OUTOFMEMORY;
   }

   pCrst->QueryInterface(IID_IHostCrst, (void**) ppCrst);
   return S_OK;
}

STDMETHODIMP SHSyncManager::CreateCrstWithSpinCount(/* in */ DWORD dwSpinCount, /* out */ IHostCrst **ppCrst) {
   Logger::Info("In SyncManager::CreateCrstWithSpinCount");

   IHostCrst* pCrst = new SHCrst(dwSpinCount);
   if (!pCrst) {
      Logger::Error("Failed to allocate a new CrstWithSpinCount");
      *ppCrst = NULL;
      return E_OUTOFMEMORY;
   }

   pCrst->QueryInterface(IID_IHostCrst, (void**) ppCrst);
   return S_OK;
}

STDMETHODIMP SHSyncManager::CreateAutoEvent(/* out */IHostAutoEvent **ppEvent) {
   Logger::Info("In SyncManager::CreateAutoEvent");

   SHAutoEvent* pEvent = new SHAutoEvent(-1);
   if (!pEvent) {
      Logger::Error("Failed to allocate a new AutoEvent");
      *ppEvent = NULL;
      return E_OUTOFMEMORY;
   }

   pEvent->QueryInterface(IID_IHostAutoEvent, (void**) ppEvent);
   return S_OK;
}

STDMETHODIMP SHSyncManager::CreateManualEvent(/* in */ BOOL bInitialState, /* out */ IHostManualEvent **ppEvent) {
   Logger::Info("In SyncManager::CreateManualEvent");

   SHManualEvent* pEvent = new SHManualEvent(bInitialState);
   if (!pEvent) {
      Logger::Error("Failed to allocate a new ManualEvent");
      *ppEvent = NULL;
      return E_OUTOFMEMORY;
   }

   pEvent->QueryInterface(IID_IHostManualEvent, (void**) ppEvent);
   return S_OK;
}

STDMETHODIMP SHSyncManager::CreateMonitorEvent(/* in */ SIZE_T Cookie, /* out */ IHostAutoEvent **ppEvent) {
   Logger::Info("In SyncManager::CreateMonitorEvent");

   SHAutoEvent* pEvent = new SHAutoEvent(Cookie);
   if (!pEvent) {
      Logger::Error("Failed to allocate a new AutoEvent");
      *ppEvent = NULL;
      return E_OUTOFMEMORY;
   }

   pEvent->SetContext(m_pContext);
   pEvent->QueryInterface(IID_IHostAutoEvent, (void**) ppEvent);

   return S_OK;
}

STDMETHODIMP SHSyncManager::CreateRWLockWriterEvent(/* in */ SIZE_T Cookie, /* out */ IHostAutoEvent **ppEvent) {
   Logger::Info("In SyncManager::CreateRWLockWriterEvent");

   SHAutoEvent* pEvent = new SHAutoEvent(-1);
   if (!pEvent) {
      Logger::Error("Failed to allocate a new AutoEvent");
      *ppEvent = NULL;
      return E_OUTOFMEMORY;
   }

   pEvent->QueryInterface(IID_IHostAutoEvent, (void**) ppEvent);
   return S_OK;
}

STDMETHODIMP SHSyncManager::CreateRWLockReaderEvent(/* in */ BOOL bInitialState, /* in */ SIZE_T Cookie, /* out */ IHostManualEvent **ppEvent) {
   Logger::Info("In SyncManager::CreateRWLockReaderEvent");

   SHAutoEvent* pEvent = new SHAutoEvent(-1);
   if (!pEvent) {
      Logger::Error("Failed to allocate a new AutoEvent");
      *ppEvent = NULL;
      return E_OUTOFMEMORY;
   }

   pEvent->QueryInterface(IID_IHostAutoEvent, (void**) ppEvent);
   return S_OK;
}

STDMETHODIMP SHSyncManager::CreateSemaphore(/* in */ DWORD dwInitial, /* in */ DWORD dwMax, /* out */ IHostSemaphore **ppSemaphore) {
   Logger::Info("In SyncManager::CreateSemaphore");
   
   SHSemaphore* pSemaphore = new SHSemaphore(dwInitial, dwMax);
   if (!pSemaphore) {
      Logger::Error("Failed to allocate a new Semaphore");
      *ppSemaphore = NULL;
      return E_OUTOFMEMORY;
   }

   pSemaphore->QueryInterface(IID_IHostSemaphore, (void**) ppSemaphore);
   return S_OK;
}
