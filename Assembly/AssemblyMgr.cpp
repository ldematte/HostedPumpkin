

#include "AssemblyMgr.h"
#include "AssemblyStore.h"

#include "../Logger.h"

SHAssemblyManager::SHAssemblyManager(const std::list<AssemblyInfo>& hostAssemblies) {
   m_cRef = 0;
   m_hostAssemblies = hostAssemblies;
   assemblyStore = NULL;
}

SHAssemblyManager::~SHAssemblyManager() {}

// IUnknown functions

STDMETHODIMP_(DWORD) SHAssemblyManager::AddRef() {
   return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(DWORD) SHAssemblyManager::Release() {
   ULONG cRef = InterlockedDecrement(&m_cRef);
   if (cRef == 0) {
      delete this;
   }
   return cRef;
}

STDMETHODIMP SHAssemblyManager::QueryInterface(const IID &riid, void **ppvObject) {
   if (!ppvObject)
      return E_POINTER;

   if (riid == IID_IUnknown || riid == IID_IHostAssemblyManager) {
      *ppvObject = this;
      AddRef();
      return S_OK;
   }

   *ppvObject = NULL;
   return E_NOINTERFACE;
}

// IHostGCManager functions

STDMETHODIMP SHAssemblyManager::GetNonHostStoreAssemblies(/* [out] */ ICLRAssemblyReferenceList **ppReferenceList) {
   Logger::Info("In AssemblyManager::GetNonHostStoreAssemblies");
   // Tell the CLR to just try to load everything by itself from the GAC, as a first step...
   *ppReferenceList = NULL;
   return S_OK;
}

STDMETHODIMP SHAssemblyManager::GetAssemblyStore(/* [out] */ IHostAssemblyStore **ppAssemblyStore) {
   Logger::Info("In AssemblyManager::GetAssemblyStore");

   // ... if not, try to load it using our store

   if (assemblyStore == NULL) {
      assemblyStore = new SHAssemblyStore(&m_hostAssemblies);
   }

   *ppAssemblyStore = assemblyStore;

   // not something we are interested in?
   // Continue with the standart app base probe
   return S_OK;

}
