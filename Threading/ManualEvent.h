
#ifndef SH_MANUAL_EVENT_H_INCLUDED
#define SH_MANUAL_EVENT_H_INCLUDED

#include "../Common.h"

class SHManualEvent : public IHostManualEvent {
private:
   volatile LONG m_cRef;
   HANDLE m_hEvent;

public:
   SHManualEvent(BOOL bInitialState);
   ~SHManualEvent();

   // IUnknown functions
   STDMETHODIMP_(DWORD) AddRef();
   STDMETHODIMP_(DWORD) Release();
   STDMETHODIMP QueryInterface(const IID &riid, void **ppvObject);

   // IHostManualEvent functions
   STDMETHODIMP Wait(DWORD dwMilliseconds, DWORD option);
   STDMETHODIMP Reset();
   STDMETHODIMP Set();
};

#endif //SH_MANUAL_EVENT_H_INCLUDED
