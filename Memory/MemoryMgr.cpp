
#include "MemoryMgr.h"
#include "Malloc.h"

#include "../Logger.h"


// TODO: Use memoryNotificationCallback to notify the CLR when memory is low
SHMemoryManager::SHMemoryManager() {
   m_cRef = 0;
   memoryNotificationCallback = NULL;
}

SHMemoryManager::~SHMemoryManager() {
   if (memoryNotificationCallback)
      memoryNotificationCallback->Release();
}

// IUnknown functions

STDMETHODIMP_(DWORD) SHMemoryManager::AddRef() {
   return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(DWORD) SHMemoryManager::Release() {
   ULONG cRef = InterlockedDecrement(&m_cRef);
   if (cRef == 0) {
      delete this;
   }
   return cRef;
}

STDMETHODIMP SHMemoryManager::QueryInterface(const IID &riid, void **ppvObject) {
   if (riid == IID_IUnknown || riid == IID_IHostMemoryManager) {
      *ppvObject = this;
      AddRef();
      return S_OK;
   }

   *ppvObject = NULL;
   return E_NOINTERFACE;
}

// IHostMemoryManager functions


STDMETHODIMP SHMemoryManager::CreateMalloc(DWORD dwMallocType, IHostMalloc **ppMalloc) {
   *ppMalloc = new SHMalloc(dwMallocType);
   // TODO: check return value from SHMalloc "constructor"? (make a init function?)
   return S_OK;
}

STDMETHODIMP SHMemoryManager::VirtualAlloc(void *pAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect, EMemoryCriticalLevel eCriticalLevel, void **ppMem) {
   Logger::Info("VirtualAlloc: %d bytes, critical level %d", dwSize, eCriticalLevel);
   *ppMem = ::VirtualAlloc(pAddress, dwSize, flAllocationType, flProtect);
   if (*ppMem == NULL) {
      DWORD errorCode = GetLastError();
      Logger::Error("VirtualAlloc error: %d", errorCode);
      return HRESULT_FROM_WIN32(errorCode);
   }
   return S_OK;
}

STDMETHODIMP SHMemoryManager::VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
   Logger::Info("VirtualFree: %d bytes", dwSize);
   if (::VirtualFree(lpAddress, dwSize, dwFreeType))
      return S_OK;
   else {
      DWORD errorCode = GetLastError();
      Logger::Error("VirtualFree error: %d", errorCode);
      return HRESULT_FROM_WIN32(errorCode);
   }
}

STDMETHODIMP SHMemoryManager::VirtualQuery(void *lpAddress, void *lpBuffer, SIZE_T dwLength, SIZE_T *pResult) {
   Logger::Info("VirtualQuery: at address 0x%x", lpAddress);
   *pResult = ::VirtualQuery(lpAddress, (PMEMORY_BASIC_INFORMATION) lpBuffer, dwLength);
   if (*pResult == NULL) {
      DWORD errorCode = GetLastError();
      Logger::Error("VirtualQuery error: %d", errorCode);
      return HRESULT_FROM_WIN32(errorCode);
   }

   return S_OK;

}
STDMETHODIMP SHMemoryManager::VirtualProtect(void *lpAddress, SIZE_T dwSize, DWORD flNewProtect, DWORD *pflOldProtect) {
   Logger::Info("VirtualProtect: at address 0x%x", lpAddress);
   if (::VirtualProtect(lpAddress, dwSize, flNewProtect, pflOldProtect))
      return S_OK;
   else {
      DWORD errorCode = GetLastError();
      Logger::Error("VirtualQuery error: %d", errorCode);
      return HRESULT_FROM_WIN32(errorCode);
   }
}

STDMETHODIMP SHMemoryManager::GetMemoryLoad(DWORD *pMemoryLoad, SIZE_T *pAvailableBytes) {
   Logger::Info("In GetGetMemoryLoad");
   MEMORYSTATUS memoryStatus;
   GlobalMemoryStatus(&memoryStatus);

   *pMemoryLoad = memoryStatus.dwMemoryLoad;
   *pAvailableBytes = memoryStatus.dwAvailVirtual; // TODO: restrict to "available physical?
   return S_OK;
}

STDMETHODIMP SHMemoryManager::RegisterMemoryNotificationCallback(ICLRMemoryNotificationCallback *pCallback) {
   this->memoryNotificationCallback = pCallback;
   return S_OK;
}


// These are just reporting functions: the CLR still does file mapping by itself, but it is kind enough to inform
// us of where and how much
STDMETHODIMP SHMemoryManager::NeedsVirtualAddressSpace(LPVOID startAddress, SIZE_T size) {
   // MapViewOfFile failed, and the CLR asks us if we can do something...
   Logger::Info("In NeedsVirtualAddressSpace (MapViewOfFile failed) at %x for %d bytes", startAddress, size);
   // I don't think so
   return S_FALSE;
}

STDMETHODIMP SHMemoryManager::AcquiredVirtualAddressSpace(LPVOID startAddress, SIZE_T size) {
   Logger::Info("In AcquiredVirtualAddressSpace (MapViewOfFile called) at %x for %d bytes", startAddress, size);
   return S_OK;
}

STDMETHODIMP SHMemoryManager::ReleasedVirtualAddressSpace(LPVOID startAddress) {
   Logger::Info("In ReleasedVirtualAddressSpace (UnmapViewOfFile called) at %x", startAddress);
   return S_OK;
}
