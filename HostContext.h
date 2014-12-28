
#ifndef CONTEXT_H_INCLUDED
#define CONTEXT_H_INCLUDED

#include "Common.h"

#include <map>

#import "SimpleHostRuntime.tlb" no_namespace named_guids

//using namespace SimpleHostRuntime;

#include "AppDomainInfo.h"

class HostContext: public IHostContext {
private:
   volatile LONG m_cRef;

   LPCRITICAL_SECTION domainMapCrst;
   std::map<DWORD, AppDomainInfo> appDomains;

   std::map<DWORD, DWORD> parentChildThread;
   std::map<DWORD, DWORD> threadAppDomain;


   volatile unsigned long numZombieDomains;

   ISimpleHostDomainManager* defaultDomainManager;

public:
   HostContext();
   virtual ~HostContext();

   // IUnknown functions
   STDMETHODIMP_(DWORD) AddRef();
   STDMETHODIMP_(DWORD) Release();
   STDMETHODIMP QueryInterface(const IID &riid, void **ppvObject);

   // IHostContext functions
   virtual STDMETHODIMP raw_GetThreadCount(
      /*[in]*/ long appDomainId,
      /*[out,retval]*/ long * pRetVal);

   void OnDomainUnload(DWORD domainId);
   void OnDomainRudeUnload();
   void OnDomainCreate(DWORD domainId, DWORD dwCurrentThreadId, ISimpleHostDomainManager* domainManager);
   ISimpleHostDomainManager* GetDomainManagerForDefaultDomain();

   // Notifies that the managed code "got hold" (created, got from a pool) of a new thread   
   bool OnThreadAcquire(DWORD dwParentThreadId, DWORD dwNewThreadId);
   bool OnThreadRelease(DWORD dwThreadId);

   bool OnMemoryAlloc(DWORD dwThreadId, LONG bytes, PVOID address);
   bool OnMemoryFree(LONG bytes, PVOID address);

  
   static HRESULT HostWait(HANDLE hWait, DWORD dwMilliseconds, DWORD dwOption);
   static HRESULT Sleep(DWORD dwMilliseconds, DWORD dwOption);
   static HRESULT HRESULTFromWaitResult(DWORD dwWaitResult);

   // Check the "status" of a Snippet-AppDomain
   // TODO: consider using ICLRAppDomainResourceMonitor
   // http://msdn.microsoft.com/en-us/library/vstudio/dd627196%28v=vs.100%29.aspx

};

#endif //CONTEXT_H_INCLUDED
