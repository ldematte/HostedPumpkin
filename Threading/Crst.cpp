#include "Crst.h"

#include "../Logger.h"

// IUnknown functions
SHCrst::SHCrst() {
   m_cRef = 0;
   m_pCrst = new CRITICAL_SECTION;
   __try {
      InitializeCriticalSection(m_pCrst);
   }
   __except (STATUS_NO_MEMORY) {
      Logger::Critical("Failed to initialize CRST: %d", GetLastError());
   }
}

SHCrst::SHCrst(DWORD dwSpinCount) {
   m_cRef = 0;
   m_pCrst = new CRITICAL_SECTION;
   if (!InitializeCriticalSectionAndSpinCount(m_pCrst, dwSpinCount)) {
      Logger::Critical("Failed to initialize CRST: %d", GetLastError());
   }
}

SHCrst::~SHCrst() {
   if (m_pCrst) {
      DeleteCriticalSection(m_pCrst);
      m_pCrst = NULL;
   }
}

STDMETHODIMP_(DWORD) SHCrst::AddRef() {
   return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(DWORD) SHCrst::Release() {
   ULONG cRef = InterlockedDecrement(&m_cRef);
   if (cRef == 0)
      delete this;
   return cRef;
}

STDMETHODIMP SHCrst::QueryInterface(const IID &riid, void **ppvObject) {
   if (riid == IID_IUnknown || riid == IID_IHostCrst) {
      *ppvObject = this;
      AddRef();
      return S_OK;
   }

   *ppvObject = NULL;
   return E_NOINTERFACE;
}

// IHostCrst functions

STDMETHODIMP SHCrst::Enter(DWORD /*option*/) {
   //Logger::Info("In CriticalSection::Enter");

   // TODO: consider 'option' correctly.
   EnterCriticalSection(m_pCrst);
   // TODO: retval transformation

   return S_OK;
}

STDMETHODIMP SHCrst::Leave() {
   //Logger::Info("In CriticalSection::Leave");

   LeaveCriticalSection(m_pCrst);
   // TODO: retval transformation

   return S_OK;
}

STDMETHODIMP SHCrst::TryEnter(DWORD /*option*/, BOOL *pbSucceeded) {
   //Logger::Info("In CriticalSection::TryEnter");

   //TODO consider 'option' correctly.
   *pbSucceeded = TryEnterCriticalSection(m_pCrst);
   // TODO: retval transformation

   return S_OK;
}

STDMETHODIMP SHCrst::SetSpinCount(DWORD dwSpinCount) {
   Logger::Info("In CriticalSection::SetSpinCount");

   SetCriticalSectionSpinCount(m_pCrst, dwSpinCount);
   // TODO: retval transformation

   return S_OK;
}