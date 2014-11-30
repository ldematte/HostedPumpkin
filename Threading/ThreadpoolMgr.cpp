
#include "ThreadpoolMgr.h"
#include "../Logger.h"



SHThreadpoolManager::SHThreadpoolManager() {
   m_cRef = 0;

   //Custom max number of threads
   maxThreads = 0;
}

SHThreadpoolManager::~SHThreadpoolManager() {
}

// IUnknown functions

STDMETHODIMP_(DWORD) SHThreadpoolManager::AddRef()
{
   return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(DWORD) SHThreadpoolManager::Release()
{
   ULONG cRef = InterlockedDecrement(&m_cRef);
   if (cRef == 0)
   {
      delete this;
   }
   return cRef;
}

STDMETHODIMP SHThreadpoolManager::QueryInterface(const IID &riid, void **ppvObject)
{
   if (riid == IID_IUnknown || riid == IID_IHostThreadpoolManager)
   {
      *ppvObject = this;
      AddRef();
      return S_OK;
   }

   *ppvObject = NULL;
   return E_NOINTERFACE;
}

// IHostThreadpoolManager functions

// This implementation uses the standard threadpool.
// Alternatively, it may be possible to use the NT6 custom thread pools
// See http://msdn.microsoft.com/en-us/library/windows/desktop/ms686980%28v=vs.85%29.aspx

STDMETHODIMP SHThreadpoolManager::QueueUserWorkItem(
   /* [in] */ LPTHREAD_START_ROUTINE Function,
   /* [in] */ PVOID Context,
   /* [in] */ ULONG Flags) {

   Logger::Info("In QueueUserWorkItem");

   if (maxThreads > 0) {
      WT_SET_MAX_THREADPOOL_THREADS(Flags, maxThreads);
   }

   if (::QueueUserWorkItem(Function, Context, Flags)) {
      return S_OK;
   }
   else {
      DWORD errorCode = GetLastError();
      Logger::Error("QueueUserWorkItem error: %d", errorCode);
      return HRESULT_FROM_WIN32(errorCode);
   }
}

STDMETHODIMP SHThreadpoolManager::SetMaxThreads(
   /* [in] */ DWORD dwMaxWorkerThreads) {

   Logger::Info("SetMaxThreads: %d", dwMaxWorkerThreads);

   maxThreads = dwMaxWorkerThreads;
   return S_OK;
}

STDMETHODIMP SHThreadpoolManager::GetMaxThreads(
   /* [out] */ DWORD *pdwMaxWorkerThreads) {

   Logger::Info("In GetMaxThreads");
   if (maxThreads > 0) {
      // Return the supplied value
      *pdwMaxWorkerThreads = maxThreads;
   }
   else {
      // See http://msdn.microsoft.com/en-us/library/windows/desktop/ms684957%28v=vs.85%29.aspx
      *pdwMaxWorkerThreads = 512;      
   }

   return S_OK;
}


// We can choose to ignore the following requests 
// (and we will, since the standard Win32 API gives no way of setting/getting this info)
STDMETHODIMP SHThreadpoolManager::GetAvailableThreads(
   /* [out] */ DWORD *pdwAvailableWorkerThreads) {
   Logger::Info("In GetAvailableThreads");
   return E_NOTIMPL;
}

STDMETHODIMP SHThreadpoolManager::SetMinThreads(
   /* [in] */ DWORD dwMinIOCompletionThreads) {

   Logger::Info("In SetMinThreads");   
   return E_NOTIMPL;
}

STDMETHODIMP SHThreadpoolManager::GetMinThreads(
   /* [out] */ DWORD *pdwMinIOCompletionThreads) {

   Logger::Info("In GetMinThreads");
   return E_NOTIMPL;
}
