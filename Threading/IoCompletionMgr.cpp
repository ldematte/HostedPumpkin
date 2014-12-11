
#include "IoCompletionMgr.h"

#include "../Logger.h"

#include "../CrstLock.h"

#include <algorithm>

// Utility

int GetCpuCount() {
   SYSTEM_INFO sysInfo;
   ::GetSystemInfo(&sysInfo);
   return sysInfo.dwNumberOfProcessors;
}

SHIoCompletionManager::SHIoCompletionManager() {
   m_cRef = 0;
   clrIoCompletionManager = NULL;
   globalCompletionPort = NULL;

   numberOfPorts = 0;

   numThreads = 0;
   numBusyThreads = 0;
   numberOfProcessors = GetCpuCount();
   minThreads = numberOfProcessors;
   maxThreads = numberOfProcessors * 10;

   ZeroMemory(ports, sizeof(Port) * MAX_COMPLETION_PORTS);

   pLock = new CRITICAL_SECTION;
   if (!pLock) {
      Logger::Critical("SHIoCompletionManager: Error allocating lock");
   }

   InitializeCriticalSection(pLock);
}

SHIoCompletionManager::~SHIoCompletionManager()
{
   if (clrIoCompletionManager)
      clrIoCompletionManager->Release();

   if (pLock)
      DeleteCriticalSection(pLock);
}

// IUnknown functions

STDMETHODIMP_(DWORD) SHIoCompletionManager::AddRef()
{
   return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(DWORD) SHIoCompletionManager::Release()
{
   ULONG cRef = InterlockedDecrement(&m_cRef);
   if (cRef == 0) {
      delete this;
   }
   return cRef;
}

STDMETHODIMP SHIoCompletionManager::QueryInterface(const IID &riid, void **ppvObject)
{
   if (riid == IID_IUnknown || riid == IID_IHostIoCompletionManager)
   {
      *ppvObject = this;
      AddRef();
      return S_OK;
   }

   *ppvObject = NULL;
   return E_NOINTERFACE;
}

int SHIoCompletionManager::IndexOf(HANDLE hPort) {
   CrstLock(this->pLock);
   for (int i = 0; i < numberOfPorts; ++i) {
      if (ports[numberOfPorts].hPort == hPort) {
         return i;
      }
   }

   return -1;
}

// IHostIoCompletionManager functions
STDMETHODIMP SHIoCompletionManager::CreateIoCompletionPort(/* [out] */ HANDLE *phPort) {

   Logger::Info("In CreateIoCompletionPort");

   // TODO! if (numberOfPorts == MAX_COMPLETION_PORTS)

   *phPort = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, // No file, for now
      NULL, // There is no existing completion port (we are creating it)
      0,  // Key ignored for this case (just creation)
      0); // As many concurrent threads as processors

   if (*phPort == NULL) {
      DWORD errorCode = GetLastError();
      Logger::Error("CreateIoCompletionPort error: %d", errorCode);
      return HRESULT_FROM_WIN32(errorCode);
   }
   else {

      CrstLock(this->pLock);
      for (int i = 0; i < MAX_COMPLETION_PORTS; ++i) {
         if (ports[i].hPort == NULL) {
            ports[numberOfPorts].hPort = *phPort;
            // The number of threads must be stored in a separate memory area. 
            // We might have a race between the pool-thread func, CloseIo and Create Io otherwise
            ports[numberOfPorts].numThreads = new DWORD;
            *(ports[numberOfPorts].numThreads) = 0;
            ++numberOfPorts;
            break;
         }
      }
   }
   return S_OK;
}

STDMETHODIMP SHIoCompletionManager::CloseIoCompletionPort(HANDLE hPort) {

   Logger::Info("In CloseIoCompletionPort");

   if (::CloseHandle(hPort)) {

      CrstLock(this->pLock);
      for (int i = 0; i < numberOfPorts; ++i) {
         if (ports[numberOfPorts].hPort == hPort) {
            ports[i].hPort = NULL;
            if (ports[i].numThreads)
               delete (ports[i].numThreads);
            ports[i].numThreads = NULL;
            --numberOfPorts;
            break;
         }
      }    

      return S_OK;
   }
   else {
      DWORD errorCode = GetLastError();
      Logger::Error("CloseIoCompletionPort error: %d", errorCode);
      return HRESULT_FROM_WIN32(errorCode);
   }
}

STDMETHODIMP SHIoCompletionManager::SetMaxThreads(DWORD dwMaxIOCompletionThreads) {
   maxThreads = dwMaxIOCompletionThreads;
   return S_OK;
}

STDMETHODIMP SHIoCompletionManager::GetMaxThreads(/* [out] */ DWORD *pdwMaxIOCompletionThreads) {
   *pdwMaxIOCompletionThreads = maxThreads;
   return S_OK;
}

STDMETHODIMP SHIoCompletionManager::SetMinThreads(DWORD dwMinIOCompletionThreads) {
   minThreads = dwMinIOCompletionThreads;
   return S_OK;
}

STDMETHODIMP SHIoCompletionManager::GetMinThreads(/* [out] */ DWORD *pdwMinIOCompletionThreads) {
   *pdwMinIOCompletionThreads = minThreads;
   return S_OK;
}

STDMETHODIMP SHIoCompletionManager::GetAvailableThreads(/* [out] */ DWORD *pdwAvailableIOCompletionThreads) {
   *pdwAvailableIOCompletionThreads = maxThreads - numThreads;
  
   if (*pdwAvailableIOCompletionThreads < 0)
      *pdwAvailableIOCompletionThreads = 0;

   Logger::Info("GetAvailableThreads: returns %d", *pdwAvailableIOCompletionThreads);
   
   return S_OK;
}

STDMETHODIMP SHIoCompletionManager::SetCLRIoCompletionManager(
   /* [in] */ ICLRIoCompletionManager *pManager) {

   Logger::Info("In SetCLRIoCompletionManager");
   clrIoCompletionManager = pManager;
   return S_OK;
}

// The Windows Platform functions use the OVERLAPPED structure to store state for asynchronous I/O requests. 
// The CLR calls the InitializeHostOverlapped method to give the host the opportunity to append 
// custom data to an OVERLAPPED instance.
STDMETHODIMP SHIoCompletionManager::InitializeHostOverlapped(
   /* [in] */ void* /*pvOverlapped*/) {

   Logger::Info("In InitializeHostOverlapped");
   // We do not need to append anything, thank you :)

   return S_OK;
}

STDMETHODIMP SHIoCompletionManager::GetHostOverlappedSize(/* [out] */ DWORD *pcbSize) {
   // We do not use it, for now, so 
   *pcbSize = 0;
   return S_OK;
}

HANDLE SHIoCompletionManager::GetDefaultCompletionPort() {

   // Double-checked locking optimization
   if (globalCompletionPort == NULL) {
      CrstLock(this->pLock);
      if (globalCompletionPort == NULL) {
         HRESULT hr = CreateIoCompletionPort(&globalCompletionPort);

         if (!SUCCEEDED(hr)) {
            Logger::Critical("GetDefaultCompletionPort error: %d", hr);
         }
      }
   }

   return globalCompletionPort;
}

bool SHIoCompletionManager::HasCompletionPortThreadpool(HANDLE hPort) { 
   int idx = IndexOf(hPort);
   if (idx >= 0) {
      DWORD* p = ports[idx].numThreads;
      if (p && *p > 0)
         return true;
   }
   return false;
}

// If we had only one port, maybe we could use BindIoCompletionCallback
// http://msdn.microsoft.com/en-us/library/aa363484%28VS.85%29.aspx
// But the CLR require an arbitrary number of ports, so we need to implement it ourselves
STDMETHODIMP SHIoCompletionManager::Bind(
   /* [in] */ HANDLE hPort,
   /* [in] */ HANDLE hHandle) {

   Logger::Info("In Bind");
   if (hPort == NULL) {
      // If this is null, the CLR mean the "default completion port".
      hPort = GetDefaultCompletionPort();
   }

   // there could be a race here, but at worst we will have N threads starting up where N = number of CPUs

   // Do we have at least a thread on this completion port?
   if (!HasCompletionPortThreadpool(hPort)) {
      CreateCompletionPortThread(hPort);
   }
   else {
      GrowCompletionPortThreadpoolIfNeeded(hPort);
   }


   // Use it in the "Associate an existing I/O completion port with a file handle" mode
   HANDLE hRet = ::CreateIoCompletionPort(hHandle, // The handle that will be used to complete the request
      hPort, // The existing completion port
      0,  // Key 
      0); // Ignored

   if (hRet == NULL) {
      DWORD errorCode = GetLastError();
      Logger::Error("Bind error: %d", errorCode);
      return HRESULT_FROM_WIN32(errorCode);
   }

   return S_OK;
}

struct ThreadArgs {
   HANDLE hPort;
   SHIoCompletionManager* pManager;
   DWORD* pThreadsPerPort;
};

void SHIoCompletionManager::CreateCompletionPortThread(HANDLE hPort) {

   ThreadArgs* threadArgs = new ThreadArgs;
   threadArgs->hPort = hPort;
   threadArgs->pManager = this;
   threadArgs->pThreadsPerPort = ports[IndexOf(hPort)].numThreads;

   HANDLE hThread = CreateThread(NULL, 0, CompletionPortThreadFunc, (LPVOID)threadArgs, 0, NULL);
   
   if (hThread == NULL) {
      Logger::Error("CreateCompletionPortThread failed. Error: %d", GetLastError());
      delete threadArgs;
   }
   else {
      // We do not need the reference; close it so the thread can die when he decides to
      CloseHandle(hThread);      
   }
}

DWORD __stdcall SHIoCompletionManager::CompletionPortThreadFunc(LPVOID lpArgs) {

   const int iocpThreadWait = 1000;

   ThreadArgs* threadArgs = (ThreadArgs*) lpArgs;
   SHIoCompletionManager* me = threadArgs->pManager;

   InterlockedIncrement(&(me->numThreads));
   InterlockedIncrement(threadArgs->pThreadsPerPort);
   InterlockedIncrement(&(me->numBusyThreads));

   bool stayInPool = true;

   while (stayInPool) {
      DWORD numberOfBytes = 0;
      ULONG completionKey = 0;
      OVERLAPPED* pOverlapped;

      // We are going to wait, so we are not "busy" anymore
      InterlockedDecrement(&(me->numBusyThreads));
      BOOL success = GetQueuedCompletionStatus(threadArgs->hPort, &numberOfBytes, &completionKey, &pOverlapped, iocpThreadWait);
      DWORD dwError = GetLastError();

      // There is work to do
      InterlockedIncrement(&(me->numBusyThreads));

      if (success && pOverlapped != NULL) {
         me->clrIoCompletionManager->OnComplete(dwError, numberOfBytes, pOverlapped);
      }

      if (!success && dwError == ERROR_ABANDONED_WAIT_0) {
         // The port was closed, we need to exit this thread in any case
         stayInPool = false;
      }
      else {
         // Is there something to do?
         BOOL ioPending = TRUE;
         GetThreadIOPendingFlag(GetCurrentThread(), &ioPending);

         if (!ioPending && // There is no IO pending
            (me->numThreads > me->minThreads)) { // We have enough threads 

            // Is there another thread on this port?
            if (InterlockedDecrement(threadArgs->pThreadsPerPort) > 0) {
               stayInPool = false;
            }
            else {
               // Ops.. we hit 0
               // Roll back our decision: we stay in pool
               InterlockedIncrement(threadArgs->pThreadsPerPort);
            }
         }
      }
   }

   InterlockedDecrement(&(me->numThreads));
   // If we wanted to exit, we have already decremented threadArgs->threadsPerPort
   InterlockedDecrement(&(me->numBusyThreads));

   // Before exiting, clear args
   delete threadArgs;
   return 0;
}

void SHIoCompletionManager::GrowCompletionPortThreadpoolIfNeeded(HANDLE hPort) {

   if (numBusyThreads == numThreads && // all threads are busy
      numBusyThreads < maxThreads) {
      CreateCompletionPortThread(hPort);
   }
}
