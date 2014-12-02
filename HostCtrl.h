
#ifndef HOST_CONTROL_H_INCLUDED
#define HOST_CONTROL_H_INCLUDED

#include "Common.h"

class SHTaskManager;
class SHSyncManager;
class SHMemoryManager;
class SHThreadpoolManager;
class SHGCManager;
class SHIoCompletionManager;

class DHHostControl : public IHostControl {
private:
   volatile LONG m_cRef;
   ICLRRuntimeHost *m_pRuntimeHost;
   ICLRControl *m_pRuntimeControl;

   SHTaskManager* taskManager;
   SHSyncManager* syncManager;
   SHMemoryManager* memoryManager;
   SHThreadpoolManager* threadpoolManager;
   SHGCManager* gcManager;
   SHIoCompletionManager* iocpManager;

public:
   DHHostControl(ICLRRuntimeHost *pRuntimeHost);
   ~DHHostControl();

   ICLRControl* GetCLRControl() { return m_pRuntimeControl; };
   STDMETHODIMP_(VOID) ShuttingDown();

   // IUnknown functions
   STDMETHODIMP_(DWORD) AddRef();
   STDMETHODIMP_(DWORD) Release();
   STDMETHODIMP QueryInterface(const IID &riid, void **ppvObject);

   // IHostControl functions
   STDMETHODIMP GetHostManager(const IID &riid, void **ppObject);
   STDMETHODIMP SetAppDomainManager(DWORD dwAppDomainID, IUnknown *pUnkAppDomainManager);
};

#endif //HOST_CONTROL_H_INCLUDED
