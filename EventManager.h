
#ifndef SH_EVENT_MANAGER_H_INCLUDED
#define SH_EVENT_MANAGER_H_INCLUDED

#include "Common.h"
#include "HostContext.h"

class SHEventManager : public IActionOnCLREvent {
private:
   volatile LONG m_cRef;
   HostContext* hostContext;

public:
   SHEventManager(HostContext* hostContext);
   ~SHEventManager();

   // IUnknown functions
   STDMETHODIMP_(DWORD) AddRef();
   STDMETHODIMP_(DWORD) Release();
   STDMETHODIMP QueryInterface(const IID &riid, void **ppvObject);

   // IActionOnCLREvent functions
   STDMETHODIMP OnEvent(
      /* [in] */ EClrEvent event,
      /* [in] */ PVOID data);

};

#endif //SH_EVENT_MANAGER_H_INCLUDED
