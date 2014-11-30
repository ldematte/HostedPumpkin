
#ifndef SH_GC_MANAGER_H_INCLUDED
#define SH_GC_MANAGER_H_INCLUDED

#include "../Common.h"

class SHGCManager : public IHostGCManager {

private:
   volatile LONG m_cRef;

public:
   SHGCManager();
   ~SHGCManager();

   // IUnknown functions
   STDMETHODIMP_(DWORD) AddRef();
   STDMETHODIMP_(DWORD) Release();
   STDMETHODIMP QueryInterface(const IID &riid, void **ppvObject);

   // IHostGCManager functions
   STDMETHODIMP ThreadIsBlockingForSuspension();
   STDMETHODIMP SuspensionStarting();
   STDMETHODIMP SuspensionEnding(DWORD Generation);
};

#endif //SH_GC_MANAGER_H_INCLUDED

