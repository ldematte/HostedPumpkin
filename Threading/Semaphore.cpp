#include "Semaphore.h"

#include "../HostContext.h"
#include "../Logger.h"

// Standard functions

SHSemaphore::SHSemaphore(DWORD dwInitial, DWORD dwMax) {
   m_cRef = 0;
   m_hSemaphore = CreateSemaphore(NULL, dwInitial, dwMax, NULL);
   if (!m_hSemaphore) {
      Logger::Critical("Failed to create semaphore: %d", GetLastError());
   }
}

SHSemaphore::~SHSemaphore() {
   if (m_hSemaphore != 0) {
      CloseHandle(m_hSemaphore);
      m_hSemaphore = 0;
   }
}

// IUnknown functions

STDMETHODIMP_(DWORD) SHSemaphore::AddRef() {
   return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(DWORD) SHSemaphore::Release() {
   ULONG cRef = InterlockedDecrement(&m_cRef);
   if (cRef == 0)
      delete this;
   return cRef;
}

STDMETHODIMP SHSemaphore::QueryInterface(const IID &riid, void **ppvObject) {
   if (riid == IID_IUnknown || riid == IID_IHostSemaphore) {
      *ppvObject = this;
      AddRef();
      return S_OK;
   }

   *ppvObject = NULL;
   return E_NOINTERFACE;
}

// IHostSemaphore functions

STDMETHODIMP SHSemaphore::Wait(DWORD dwMilliseconds, DWORD option) {
   Logger::Info("In Semaphore::Wait");
   return HostContext::HostWait(m_hSemaphore, dwMilliseconds, option);
}

STDMETHODIMP SHSemaphore::ReleaseSemaphore(LONG lReleaseCount, LONG *lpPreviousCount) {
   Logger::Info("In ReleaseSemaphore");
   if (!::ReleaseSemaphore(m_hSemaphore, lReleaseCount, lpPreviousCount)) {
      DWORD error = GetLastError();
      Logger::Error("Failed to release semaphore: %d", error);
      return HRESULT_FROM_WIN32(error);
   }
   return S_OK;
}
