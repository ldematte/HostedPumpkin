
#ifndef SH_AUTO_EVENT_H_INCLUDED
#define SH_AUTO_EVENT_H_INCLUDED

#include "../Common.h"

#include "../Context.h"

class SHAutoEvent : public IHostAutoEvent
{
private:
   volatile LONG m_cRef;
   HANDLE m_hEvent;
   SIZE_T m_cookie;
   DHContext *m_pContext;

public:
   SHAutoEvent(SIZE_T cookie);
   ~SHAutoEvent();

   void SetContext(DHContext *pContext) { m_pContext = pContext; };

   // IUnknown functions
   STDMETHODIMP_(DWORD) AddRef();
   STDMETHODIMP_(DWORD) Release();
   STDMETHODIMP QueryInterface(const IID &riid, void **ppvObject);

   // IHostAutoEvent functions
   STDMETHODIMP Wait(DWORD dwMilliseconds, DWORD option);
   STDMETHODIMP Set();
};

#endif //SH_AUTO_EVENT_H_INCLUDED
