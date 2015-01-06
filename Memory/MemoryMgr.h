
#ifndef MEMORY_H_INCLUDED
#define MEMORY_H_INCLUDED

#include "../Common.h"
#include "../HostContext.h"

class SHMemoryManager : public IHostMemoryManager {

private:
   volatile LONG m_cRef;
   ICLRMemoryNotificationCallback* memoryNotificationCallback;
   HostContext* hostContext;

public:
   SHMemoryManager(HostContext* context);
   ~SHMemoryManager();

   // IUnknown functions
   STDMETHODIMP_(DWORD) AddRef();
   STDMETHODIMP_(DWORD) Release();
   STDMETHODIMP QueryInterface(const IID &riid, void **ppvObject);

   STDMETHODIMP CreateMalloc(DWORD dwMallocType, IHostMalloc **ppMalloc);
   STDMETHODIMP VirtualAlloc(void *pAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect, EMemoryCriticalLevel eCriticalLevel, void **ppMem);
   STDMETHODIMP VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);
   STDMETHODIMP VirtualQuery(void *lpAddress, void *lpBuffer, SIZE_T dwLength, SIZE_T *pResult);
   STDMETHODIMP VirtualProtect(void *lpAddress, SIZE_T dwSize, DWORD flNewProtect, DWORD *pflOldProtect);
   STDMETHODIMP GetMemoryLoad(DWORD *pMemoryLoad, SIZE_T *pAvailableBytes);
   STDMETHODIMP RegisterMemoryNotificationCallback(ICLRMemoryNotificationCallback *pCallback);
   STDMETHODIMP NeedsVirtualAddressSpace(LPVOID startAddress, SIZE_T size);
   STDMETHODIMP AcquiredVirtualAddressSpace(LPVOID startAddress, SIZE_T size);
   STDMETHODIMP ReleasedVirtualAddressSpace(LPVOID startAddress);
};

#endif //MEMORY_H_INCLUDED
