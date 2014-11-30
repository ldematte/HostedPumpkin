

#ifndef SH_THREADPOOL_MANAGER_H_INCLUDED
#define SH_THREADPOOL_MANAGER_H_INCLUDED

#include "../Common.h"

class SHThreadpoolManager : public IHostThreadpoolManager {

private:
   volatile LONG m_cRef;
   DWORD maxThreads;

public:
   SHThreadpoolManager();
   ~SHThreadpoolManager();

   // IUnknown functions
   STDMETHODIMP_(DWORD) AddRef();
   STDMETHODIMP_(DWORD) Release();
   STDMETHODIMP QueryInterface(const IID &riid, void **ppvObject);

   // IHostThreadpoolManager functions
   STDMETHODIMP QueueUserWorkItem(
      /* [in] */ LPTHREAD_START_ROUTINE Function,
      /* [in] */ PVOID Context,
      /* [in] */ ULONG Flags);

   STDMETHODIMP SetMaxThreads(
      /* [in] */ DWORD dwMaxWorkerThreads);

   STDMETHODIMP GetMaxThreads(
      /* [out] */ DWORD *pdwMaxWorkerThreads);

   STDMETHODIMP GetAvailableThreads(
      /* [out] */ DWORD *pdwAvailableWorkerThreads);

   STDMETHODIMP SetMinThreads(
      /* [in] */ DWORD dwMinIOCompletionThreads);

   STDMETHODIMP GetMinThreads(
      /* [out] */ DWORD *pdwMinIOCompletionThreads);


};

#endif //SH_THREADPOOL_MANAGER_H_INCLUDED
