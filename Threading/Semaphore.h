
#ifndef SEMAPHORE_H_INCLUDED
#define SEMAPHORE_H_INCLUDED

#include "../Common.h"

class SHSemaphore : public IHostSemaphore {
private:
   volatile LONG m_cRef;
   HANDLE m_hSemaphore;
public:
   SHSemaphore(DWORD dwInitial, DWORD dwMax);
   ~SHSemaphore();

   // IUnknown functions
   STDMETHODIMP_(DWORD) AddRef();
   STDMETHODIMP_(DWORD) Release();
   STDMETHODIMP QueryInterface(const IID &riid, void **ppvObject);

   // IHostSemaphore functions
   STDMETHODIMP Wait(DWORD dwMilliseconds, DWORD option);
   STDMETHODIMP ReleaseSemaphore(LONG lReleaseCount, LONG *lpPreviousCount);
};

#endif //SEMAPHORE_H_INCLUDED
