
#ifndef CONTEXT_H_INCLUDED
#define CONTEXT_H_INCLUDED

#include "Common.h"

#include <map>

#import "SimpleHostRuntime.tlb" no_namespace named_guids

//using namespace SimpleHostRuntime;

class HostContext {
public:

   // TODO: make private
   std::map<DWORD, ISimpleHostDomainManager*> appDomains;
   ISimpleHostDomainManager* defaultDomainManager;

   HostContext() : defaultDomainManager(NULL) { }
  
   static HRESULT HostWait(HANDLE hWait, DWORD dwMilliseconds, DWORD dwOption);
   static HRESULT Sleep(DWORD dwMilliseconds, DWORD dwOption);
   static HRESULT HRESULTFromWaitResult(DWORD dwWaitResult);
};

#endif //CONTEXT_H_INCLUDED
