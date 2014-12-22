
#ifndef CONTEXT_H_INCLUDED
#define CONTEXT_H_INCLUDED

#include "Common.h"

#include <map>

#import "SimpleHostRuntime.tlb" no_namespace named_guids

//using namespace SimpleHostRuntime;

class HostContext {
private:
   LPCRITICAL_SECTION domainMapCrst;
   std::map<DWORD, ISimpleHostDomainManager*> appDomains;

   volatile unsigned long numZombieDomains;

   ISimpleHostDomainManager* defaultDomainManager;

public:  

   HostContext();
   virtual ~HostContext();

   void OnDomainUnload(DWORD domainId);
   void OnDomainRudeUnload();
   void OnDomainCreate(DWORD domainId, ISimpleHostDomainManager* domainManager);
   ISimpleHostDomainManager* GetDomainManagerForDefaultDomain();
  
   static HRESULT HostWait(HANDLE hWait, DWORD dwMilliseconds, DWORD dwOption);
   static HRESULT Sleep(DWORD dwMilliseconds, DWORD dwOption);
   static HRESULT HRESULTFromWaitResult(DWORD dwWaitResult);

   // Check the "status" of a Snippet-AppDomain
   // TODO: consider using ICLRAppDomainResourceMonitor
   // http://msdn.microsoft.com/en-us/library/vstudio/dd627196%28v=vs.100%29.aspx

};

#endif //CONTEXT_H_INCLUDED
