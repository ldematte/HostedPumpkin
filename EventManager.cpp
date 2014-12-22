
#include "EventManager.h"
#include "Logger.h"

SHEventManager::SHEventManager(HostContext* context) {
   hostContext = context;
   m_cRef = 0;
}

SHEventManager::~SHEventManager() {}

//
// IUnknown
//
HRESULT STDMETHODCALLTYPE SHEventManager::QueryInterface(const IID &riid, void **ppv) {
   if (!ppv)
      return E_POINTER;

   if (riid == IID_IUnknown || riid == IID_IActionOnCLREvent) {
      *ppv = this;
      AddRef();
      return S_OK;
   }

   *ppv = NULL;
   return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE SHEventManager::AddRef() {
   return InterlockedIncrement(&m_cRef);
}

ULONG STDMETHODCALLTYPE SHEventManager::Release() {
   if (InterlockedDecrement(&m_cRef) == 0) {
      delete this;
      return 0;
   }
   return m_cRef;
}

//
// IActionOnCLREvent 
// 
STDMETHODIMP SHEventManager::OnEvent(EClrEvent event, PVOID data) {  

   switch (event) {
   case Event_DomainUnload: 
      Logger::Debug("In EventManager::OnEvent: Event_DomainUnload, %d", data);
      hostContext->OnDomainUnloaded((DWORD)data);
      break;

   case Event_ClrDisabled:
      Logger::Debug("In EventManager::OnEvent: Event_ClrDisabled");
      // TODO: recycle process
      break;

   case Event_MDAFired: {
         // Managed debugging assistants events. See
         // See http://msdn.microsoft.com/en-us/library/vstudio/d21c150d%28v=vs.100%29.aspx
         MDAInfo* mdaInfo = (MDAInfo*) data;
         Logger::Debug(L"In EventManager::OnEvent: Event_MDAFired : %s, %s", mdaInfo->lpMDACaption, mdaInfo->lpMDAMessage);
         Logger::Info(L"Stack trace: %s", mdaInfo->lpStackTrace);
      }
      break;

   case Event_StackOverflow:
      // TODO: do we need to take some action? Or will the escalation policies suffice?
      break;     

   }

   return S_OK;
}

