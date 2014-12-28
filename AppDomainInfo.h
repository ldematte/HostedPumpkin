
#ifndef SH_APPDOMAIN_INFO_H_INCLUDED
#define SH_APPDOMAIN_INFO_H_INCLUDED

#include "Common.h"

//#import "SimpleHostRuntime.tlb" no_namespace named_guids
struct ISimpleHostDomainManager;

struct AppDomainInfo {
   
   AppDomainInfo(DWORD threadId, ISimpleHostDomainManager* manager) 
      : mainThreadId(threadId), appDomainManager(manager) { }

   DWORD mainThreadId;
   ISimpleHostDomainManager* appDomainManager;
   LONG threadsInAppDomain;
   LONG bytesInAppDomain;

};

#endif //SH_APPDOMAIN_INFO_H_INCLUDED
