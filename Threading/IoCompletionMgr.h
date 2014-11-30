
#ifndef SH_IOCOMPLETION_MANAGER_H_INCLUDED
#define SH_IOCOMPLETION_MANAGER_H_INCLUDED

#include "../Common.h"

const int MAX_COMPLETION_PORTS = 16;

class SHIoCompletionManager : public IHostIoCompletionManager {
private:
   volatile LONG m_cRef;
   ICLRIoCompletionManager* clrIoCompletionManager;

   HANDLE globalCompletionPort;
   CRITICAL_SECTION* pLock;

   HANDLE GetDefaultCompletionPort();

   void GrowCompletionPortThreadpoolIfNeeded(HANDLE hPort);
   void CreateCompletionPortThread(HANDLE hPort);
   static DWORD __stdcall CompletionPortThreadFunc(LPVOID lpArgs);

   volatile DWORD numThreads;
   volatile DWORD numBusyThreads;
   DWORD minThreads;
   DWORD maxThreads;
   DWORD numberOfProcessors;

   struct Port {
      HANDLE hPort;
      DWORD* numThreads;
   };

   Port ports[MAX_COMPLETION_PORTS];
   int numberOfPorts;
   int IndexOf(HANDLE hPort);
   bool HasCompletionPortThreadpool(HANDLE hPort);

public:
   SHIoCompletionManager();
   ~SHIoCompletionManager();

   // IUnknown functions
   STDMETHODIMP_(DWORD) AddRef();
   STDMETHODIMP_(DWORD) Release();
   STDMETHODIMP QueryInterface(const IID &riid, void **ppvObject);

   // IHostIoCompletionManager functions
   STDMETHODIMP CreateIoCompletionPort(
      /* [out] */ HANDLE *phPort);

   STDMETHODIMP CloseIoCompletionPort(
      /* [in] */ HANDLE hPort);

   STDMETHODIMP SetMaxThreads(
      /* [in] */ DWORD dwMaxIOCompletionThreads);

   STDMETHODIMP GetMaxThreads(
      /* [out] */ DWORD *pdwMaxIOCompletionThreads);

   STDMETHODIMP GetAvailableThreads(
      /* [out] */ DWORD *pdwAvailableIOCompletionThreads);

   STDMETHODIMP GetHostOverlappedSize(
      /* [out] */ DWORD *pcbSize);

   STDMETHODIMP SetCLRIoCompletionManager(
      /* [in] */ ICLRIoCompletionManager *pManager);

   STDMETHODIMP InitializeHostOverlapped(
      /* [in] */ void *pvOverlapped);

   STDMETHODIMP Bind(
      /* [in] */ HANDLE hPort,
      /* [in] */ HANDLE hHandle);

   STDMETHODIMP SetMinThreads(
      /* [in] */ DWORD dwMinIOCompletionThreads);

   STDMETHODIMP GetMinThreads(
      /* [out] */ DWORD *pdwMinIOCompletionThreads);

};

#endif //SH_IOCOMPLETION_MANAGER_H_INCLUDED
