#include "AutoEvent.h"

#include "../Logger.h"
#include "../HostContext.h"

SHAutoEvent::SHAutoEvent(SIZE_T cookie) {
   m_cRef = 0;
   m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
   if (!m_hEvent)
      Logger::Critical("Error creating auto event: %d", GetLastError());
   m_cookie = cookie;
}

SHAutoEvent::~SHAutoEvent() {
   if (m_hEvent) {
      CloseHandle(m_hEvent);
      m_hEvent = NULL;
   }
}

// IUnknown functions
STDMETHODIMP_(DWORD) SHAutoEvent::AddRef() {
   return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(DWORD) SHAutoEvent::Release() {
   ULONG cRef = InterlockedDecrement(&m_cRef);
   if (cRef == 0)
      delete this;
   return cRef;
}

STDMETHODIMP SHAutoEvent::QueryInterface(const IID &riid, void **ppvObject) {
   if (riid == IID_IUnknown || riid == IID_IHostAutoEvent) {
      *ppvObject = this;
      AddRef();
      return S_OK;
   }

   *ppvObject = NULL;
   return E_NOINTERFACE;
}

// IHostManualEvent functions
STDMETHODIMP SHAutoEvent::Set() {
   Logger::Info("AutoEvent::Set");
   if (!SetEvent(m_hEvent)) {
      DWORD error = GetLastError();
      Logger::Error("SetEvent error: %d", error);
      return HRESULT_FROM_WIN32(error);
   }

   return S_OK;
}

STDMETHODIMP SHAutoEvent::Wait(DWORD dwMilliseconds, DWORD option) {
   Logger::Info("AutoEvent::Wait");
   return HostContext::HostWait(m_hEvent, dwMilliseconds, option);
}
