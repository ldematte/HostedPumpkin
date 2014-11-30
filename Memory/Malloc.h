
#ifndef SH_MALLOC_H_INCLUDED
#define SH_MALLOC_H_INCLUDED

#include "../Common.h"

class SHMalloc : public IHostMalloc {

private:
   volatile LONG m_cRef;
   HANDLE hHeap;
public:
   SHMalloc(DWORD dwMallocType);
   virtual ~SHMalloc();

   // IUnknown functions
   STDMETHODIMP_(DWORD) AddRef();
   STDMETHODIMP_(DWORD) Release();
   STDMETHODIMP QueryInterface(const IID &riid, void **ppvObject);

   // IHostMalloc functions
   STDMETHODIMP Alloc(
      /* [in] */ SIZE_T cbSize,
      /* [in] */ EMemoryCriticalLevel eCriticalLevel,
      /* [out] */ void **ppMem);

   STDMETHODIMP DebugAlloc(
      /* [in] */ SIZE_T cbSize,
      /* [in] */ EMemoryCriticalLevel eCriticalLevel,
      /* [annotation][in] */
      _In_   char *pszFileName,
      /* [in] */ int iLineNo,
      /* [annotation][out] */
      _Outptr_result_maybenull_  void **ppMem);

   STDMETHODIMP Free(
      /* [in] */ void *pMem);
   

};

#endif //SH_MALLOC_H_INCLUDED
