

#include "Malloc.h"

#include "../Logger.h"

#include <crtdbg.h>

SHMalloc::SHMalloc(DWORD dwMallocType) {
   m_cRef = 0;

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

// IHostMalloc functions
STDMETHODIMP SHMalloc::Alloc(
   /* [in] */ SIZE_T cbSize,
   /* [in] */ EMemoryCriticalLevel eCriticalLevel,
   /* [out] */ void **ppMem) {

   // TODO: track which thread (and appdomain!) requested this memory
   Logger::Info("Malloc from %d, %d bytes,  %d critical level", GetCurrentThreadId(), cbSize, eCriticalLevel);

   *ppMem = HeapAlloc(hHeap, 0, cbSize);
   if (ppMem == NULL) {
      Logger::Error("HeapAlloc returned NULL");
      return E_OUTOFMEMORY;
   }

   return S_OK;
}

STDMETHODIMP SHMalloc::DebugAlloc(
   /* [in] */ SIZE_T cbSize,
   /* [in] */ EMemoryCriticalLevel eCriticalLevel,
   /* [annotation][in] */
   _In_   char *pszFileName,
   /* [in] */ int iLineNo,
   /* [annotation][out] */
   _Outptr_result_maybenull_  void **ppMem) {

   // TODO: track which thread (and appdomain!) requested this memory
   Logger::Info("Malloc from %d (%s:%d), %d bytes,  %d critical level", GetCurrentThreadId(), pszFileName, iLineNo, cbSize, eCriticalLevel);

   *ppMem = HeapAlloc(hHeap, 0, cbSize);
   if (ppMem == NULL) {
      Logger::Error("HeapAlloc returned NULL");
      return E_OUTOFMEMORY;
   }

   return S_OK;

}

STDMETHODIMP SHMalloc::Free(void *pMem) {
   HeapFree(hHeap, 0, pMem);
   return S_OK;

}
