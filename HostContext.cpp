
#include "HostContext.h"

#include "Threading\Task.h"
#include "Threading\TaskMgr.h"
#include "Threading\SyncMgr.h"

#include "CrstLock.h"
#include "Logger.h"

#include <mscoree.h>
#include <corerror.h>


HostContext::HostContext() {
   defaultDomainManager = NULL;
   domainMapCrst = new CRITICAL_SECTION;

   numZombieDomains = 0;

   if (!domainMapCrst)
      Logger::Critical("Failed to allocate critical sections");

   InitializeCriticalSection(domainMapCrst);
}

HostContext::~HostContext() {
   if (domainMapCrst) DeleteCriticalSection(domainMapCrst);
}

void HostContext::OnDomainUnload(DWORD domainId) {

   CrstLock(this->domainMapCrst);
   auto domainIt = appDomains.find(domainId);
   if (domainIt != appDomains.end()) {
      appDomains.erase(domainIt);
   }

}

void HostContext::OnDomainRudeUnload() {
   //CrstLock(this->domainMapCrst);
   InterlockedIncrement(&numZombieDomains);
}

void HostContext::OnDomainCreate(DWORD dwAppDomainID, ISimpleHostDomainManager* domainManager) {

   CrstLock(this->domainMapCrst);
   appDomains.insert(std::make_pair(dwAppDomainID, domainManager));
   if (defaultDomainManager == NULL)
      defaultDomainManager = domainManager;
   
}

ISimpleHostDomainManager* HostContext::GetDomainManagerForDefaultDomain() {
   if (defaultDomainManager)
      defaultDomainManager->AddRef();

   return defaultDomainManager;
}

HRESULT HostContext::Sleep(DWORD dwMilliseconds, DWORD option) {

   BOOL alertable = option & WAIT_ALERTABLE;

   // WAIT_MSGPUMP: Notifies the host that it must pump messages on the current OS thread if the thread becomes blocked.The runtime specifies this value only on an STA thread.
   if (option & WAIT_MSGPUMP) {
      // There is a nice, general solution from Raymond Chen: http://blogs.msdn.com/b/oldnewthing/archive/2006/01/26/517849.aspx
      // BUT
      // according to Chris Brumme, we need NOT to pump too much!
      // http://blogs.msdn.com/b/cbrumme/archive/2003/04/17/51361.aspx
      // So, we use a simpler solution, since we also assume to be on XP or later: delegate to CoWaitForMultipleHandles
      // http://blogs.msdn.com/b/cbrumme/archive/2004/02/02/66219.aspx
      // http://msdn.microsoft.com/en-us/library/windows/desktop/ms680732%28v=vs.85%29.aspx
      DWORD dwFlags = 0;
      DWORD dwIndex;

      // if working with windows store, ensure
      // DWORD dwFlags = COWAIT_DISPATCH_CALLS | COWAIT_DISPATCH_WINDOW_MESSAGES;

      if (alertable) {
         dwFlags |= COWAIT_ALERTABLE;
         // See http://msdn.microsoft.com/en-us/library/windows/desktop/ms680732%28v=vs.85%29.aspx
         SetLastError(ERROR_SUCCESS);
      }

      // CoWaitForMultipleHandles may call WaitForMultipleObjectEx, which does NOT support 0 array handles.
      // Besides, MSDN is not clear if the handle array can be NULL, so.. we use a trick
      HANDLE dummy = GetCurrentProcess();
      HRESULT hr = CoWaitForMultipleHandles(dwFlags, dwMilliseconds, 1, &dummy, &dwIndex);
      if (dwIndex == WAIT_TIMEOUT) {
         return S_OK;
      }
      return hr;
   }
   else {
      return HRESULTFromWaitResult(SleepEx(dwMilliseconds, alertable));
   }
}

HRESULT HostContext::HRESULTFromWaitResult(DWORD dwWaitResult) {
   switch (dwWaitResult) {
   case WAIT_OBJECT_0:
      return S_OK;
   case WAIT_ABANDONED:
      return HOST_E_ABANDONED;
   case WAIT_IO_COMPLETION:
      return HOST_E_INTERRUPTED;
   case WAIT_TIMEOUT:
      return HOST_E_TIMEOUT;
   case WAIT_FAILED: {
      DWORD error = GetLastError();
      Logger::Error("Wait failed: %d", error);
      return HRESULT_FROM_WIN32(error);
   }
   default:
      Logger::Error("Unexpected wait result: %d", dwWaitResult);
      return E_FAIL;
   }
}

HRESULT HostContext::HostWait(HANDLE hWait, DWORD dwMilliseconds, DWORD dwOption) {

   BOOL alertable = dwOption & WAIT_ALERTABLE;
   if (dwOption & WAIT_MSGPUMP) {
      DWORD dwFlags = 0;
      if (alertable) {
         dwFlags |= COWAIT_ALERTABLE;
         // See http://msdn.microsoft.com/en-us/library/windows/desktop/ms680732%28v=vs.85%29.aspx
         SetLastError(ERROR_SUCCESS);
      }
      return CoWaitForMultipleHandles(dwFlags, dwMilliseconds, 1, &hWait, NULL);
   }
   else {
      return HRESULTFromWaitResult(WaitForSingleObjectEx(hWait, dwMilliseconds, alertable));
   }
}
