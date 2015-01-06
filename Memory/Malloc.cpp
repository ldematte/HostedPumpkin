

#include "Malloc.h"

#include "../Logger.h"

#include <crtdbg.h>

SHMalloc::SHMalloc(DWORD dwMallocType, HostContext* context) {
   m_cRef = 0;
   hostContext = context;

   DWORD options = 0;

   if (dwMallocType & MALLOC_EXECUTABLE) {
      options |= HEAP_CREATE_ENABLE_EXECUTE;
   }
   if (!(dwMallocType & MALLOC_THREADSAFE)) {
      // Not Thread-safe? you can turn off heap serialization
      // But we rather not..
      //options |= HEAP_NO_SERIALIZE;
   }

   hHeap = HeapCreate(options, 0, 0);
}

SHMalloc::~SHMalloc() {
   HeapDestroy(hHeap);
}


// IUnknown functions
STDMETHODIMP_(DWORD) SHMalloc::AddRef()
{
   return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(DWORD) SHMalloc::Release()
{
   ULONG cRef = InterlockedDecrement(&m_cRef);
   if (cRef == 0)
   {
      delete this;
   }
   return cRef;
}

STDMETHODIMP SHMalloc::QueryInterface(const IID &riid, void **ppvObject)
{
   if (riid == IID_IUnknown || riid == IID_IHostMalloc)
   {
      *ppvObject = this;
      AddRef();
      return S_OK;
   }

   *ppvObject = NULL;
   return E_NOINTERFACE;
}

inline HRESULT SHMalloc::InternalAlloc(DWORD dwThreadId, SIZE_T cbSize, EMemoryCriticalLevel eCriticalLevel, void **ppMem) {
   bool belowMemoryLimit = hostContext->OnMemoryAcquiring(dwThreadId, cbSize);

   *ppMem = NULL;
   if (eCriticalLevel > eTaskCritical || belowMemoryLimit) {
      *ppMem = HeapAlloc(hHeap, 0, cbSize);
      if (*ppMem == NULL) {
         Logger::Error("HeapAlloc NULL");
         return E_OUTOFMEMORY;
      }
      else {
         hostContext->OnMemoryAcquire(dwThreadId, cbSize, *ppMem);
         return S_OK;
      }
   }
   else {
      Logger::Error("Above memory limits, and critical level is too low. Returning E_OUTOFMEMORY");
      return E_OUTOFMEMORY;
   }
}

// IHostMalloc functions
STDMETHODIMP SHMalloc::Alloc(
   /* [in] */ SIZE_T cbSize,
   /* [in] */ EMemoryCriticalLevel eCriticalLevel,
   /* [out] */ void **ppMem) {

   DWORD dwThreadId = GetCurrentThreadId();

   Logger::Info("Malloc from %d, %d bytes,  %d critical level", dwThreadId, cbSize, eCriticalLevel);
   // Track which thread (and appdomain!) requested this memory
   return InternalAlloc(dwThreadId, cbSize, eCriticalLevel, ppMem);
   
}

STDMETHODIMP SHMalloc::DebugAlloc(
   /* [in] */ SIZE_T cbSize,
   /* [in] */ EMemoryCriticalLevel eCriticalLevel,
   /* [annotation][in] */
   _In_   char *pszFileName,
   /* [in] */ int iLineNo,
   /* [annotation][out] */
   _Outptr_result_maybenull_  void **ppMem) {

   DWORD dwThreadId = GetCurrentThreadId();

   Logger::Info("Malloc from %d (%s:%d), %d bytes,  %d critical level", GetCurrentThreadId(), pszFileName, iLineNo, cbSize, eCriticalLevel);
   // Track which thread (and appdomain!) requested this memory
   return InternalAlloc(dwThreadId, cbSize, eCriticalLevel, ppMem);
}

STDMETHODIMP SHMalloc::Free(void *pMem) {
   HeapFree(hHeap, 0, pMem);
   hostContext->OnMemoryRelease(pMem);
   return S_OK;
}
