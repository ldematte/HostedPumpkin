#include "GCMgr.h"

#include "../Logger.h"


SHGCManager::SHGCManager()
{
   m_cRef = 0;
}

SHGCManager::~SHGCManager()
{
}

// IUnknown functions

STDMETHODIMP_(DWORD) SHGCManager::AddRef()
{
   return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(DWORD) SHGCManager::Release()
{
   ULONG cRef = InterlockedDecrement(&m_cRef);
   if (cRef == 0)
   {
      delete this;
   }
   return cRef;
}

STDMETHODIMP SHGCManager::QueryInterface(const IID &riid, void **ppvObject)
{
   if (riid == IID_IUnknown || riid == IID_IHostGCManager)
   {
      *ppvObject = this;
      AddRef();
      return S_OK;
   }

   *ppvObject = NULL;
   return E_NOINTERFACE;
}

// IHostGCManager functions
STDMETHODIMP SHGCManager::ThreadIsBlockingForSuspension() {
   Logger::Info("In ThreadIsBlockingForSuspension");
   return S_OK;
}

STDMETHODIMP SHGCManager::SuspensionStarting() {
   Logger::Info("In SuspensionStarting");
   return S_OK;
}

STDMETHODIMP SHGCManager::SuspensionEnding(DWORD Generation) {
   Logger::Info("In SuspensionStarting: Generation %d", Generation);
   return S_OK;
}
