
#include "PolicyManager.h"
#include "Logger.h"

SHPolicyManager::SHPolicyManager(HostContext* context) {
   hostContext = context;
}

SHPolicyManager::~SHPolicyManager() {}

// IUnknown
STDMETHODIMP_(DWORD) SHPolicyManager::AddRef() {
   return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(DWORD) SHPolicyManager::Release() {
   ULONG cRef = InterlockedDecrement(&m_cRef);
   if (cRef == 0)
      delete this;
   return cRef;
}

STDMETHODIMP SHPolicyManager::QueryInterface(const IID &riid, void **ppvObject) {
   if (ppvObject == NULL)
      return E_INVALIDARG;

   if (riid == IID_IUnknown || riid == IID_IHostPolicyManager) {
      *ppvObject = this;
      AddRef();
      return S_OK;
   }

   *ppvObject = NULL;
   return E_NOINTERFACE;
}


// IHostPolicyManager
STDMETHODIMP SHPolicyManager::OnDefaultAction(
   /* [in] */ EClrOperation operation,
   /* [in] */ EPolicyAction action) {
   Logger::Debug("In PolicyManager::OnDefaultAction %d - %d", operation, action);

   if (action == eRudeUnloadAppDomain)
      hostContext->OnDomainRudeUnload();
   
   return S_OK;
}

STDMETHODIMP SHPolicyManager::OnTimeout(
   /* [in] */ EClrOperation operation,
   /* [in] */ EPolicyAction action) {
   Logger::Debug("In PolicyManager::OnTimeout %d - %d", operation, action);

   if (action == eRudeUnloadAppDomain)
      hostContext->OnDomainRudeUnload();

   return S_OK;
}

STDMETHODIMP SHPolicyManager::OnFailure(
   /* [in] */ EClrFailure failure,
   /* [in] */ EPolicyAction action) {
   Logger::Debug("In PolicyManager::OnFailure %d - %d", failure, action);

   if (action == eRudeUnloadAppDomain)
      hostContext->OnDomainRudeUnload();

   return S_OK;
}


