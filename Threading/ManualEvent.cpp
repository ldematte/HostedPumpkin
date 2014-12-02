#include "ManualEvent.h"

#include "../Logger.h"
#include "../HostContext.h"

SHManualEvent::SHManualEvent(BOOL bInitialState) {
   m_cRef = 0;
   m_hEvent = CreateEvent(NULL, TRUE, bInitialState, NULL);
   if (!m_hEvent)
      Logger::Critical("Error creating manual event: %d", GetLastError());
}

SHManualEvent::~SHManualEvent() {
   if (m_hEvent) {
      CloseHandle(m_hEvent);
      m_hEvent = NULL;
   }
}

// IUnknown functions
STDMETHODIMP_(DWORD) SHManualEvent::AddRef() {
   return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(DWORD) SHManualEvent::Release() {
   ULONG cRef = InterlockedDecrement(&m_cRef);
   if (cRef == 0)
      delete this;
   return cRef;
}

STDMETHODIMP SHManualEvent::QueryInterface(const IID &riid, void **ppvObject) {
   if (riid == IID_IUnknown || riid == IID_IHostManualEvent) {
      *ppvObject = this;
      AddRef();
      return S_OK;
   }

   *ppvObject = NULL;
   return E_NOINTERFACE;
}

// IHostManualEvent functions

STDMETHODIMP SHManualEvent::Set() {
   Logger::Info("ManualEvent::Set");
   if (!SetEvent(m_hEvent)) {
      DWORD error = GetLastError();
      Logger::Error("SetEvent error: %d", error);
      return HRESULT_FROM_WIN32(error);
   }

   return S_OK;
}

STDMETHODIMP SHManualEvent::Reset() {
   Logger::Info("ManualEvent::Reset");
   if (!ResetEvent(m_hEvent)) {
      DWORD error = GetLastError();
      Logger::Error("Reset error: %d", error);
      return HRESULT_FROM_WIN32(error);
   }

   return S_OK;
}

STDMETHODIMP SHManualEvent::Wait(DWORD dwMilliseconds, DWORD option) {
   Logger::Info("ManualEvent::Wait");
   return HostContext::HostWait(m_hEvent, dwMilliseconds, option);
}
