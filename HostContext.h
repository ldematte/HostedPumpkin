
#ifndef CONTEXT_H_INCLUDED
#define CONTEXT_H_INCLUDED

#include "Common.h"

class HostContext {
public:
  
   static HRESULT HostWait(HANDLE hWait, DWORD dwMilliseconds, DWORD dwOption);
   static HRESULT Sleep(DWORD dwMilliseconds, DWORD dwOption);
   static HRESULT HRESULTFromWaitResult(DWORD dwWaitResult);
};

#endif //CONTEXT_H_INCLUDED
