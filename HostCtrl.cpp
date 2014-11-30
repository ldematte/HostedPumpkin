
#include <metahost.h>

#include "HostCtrl.h"
#include "Threading\TaskMgr.h"
#include "Threading\SyncMgr.h"
#include "Memory\MemoryMgr.h"
#include "Memory\GCMgr.h"
#include "Threading\ThreadpoolMgr.h"
#include "Threading\IoCompletionMgr.h"

#include "Logger.h"

DHHostControl::DHHostControl(ICLRRuntimeHost *pRuntimeHost)
{
    m_cRef = 0;
    m_pRuntimeHost = pRuntimeHost;
    m_pRuntimeHost->AddRef();

    if (!SUCCEEDED(pRuntimeHost->GetCLRControl(&m_pRuntimeControl)))
    {
        Logger::Critical("Failed to obtain a CLR Control object");
    }

    m_pContext = new DHContext();
    m_pTaskManager = new SHTaskManager(m_pContext);
    m_pSyncManager = new SHSyncManager(m_pContext);
    memoryManager = new SHMemoryManager();
    gcManager = new SHGCManager();
    threadpoolManager = new SHThreadpoolManager();
    iocpManager = new SHIoCompletionManager();


    if (!m_pContext || !m_pTaskManager || !m_pSyncManager || 
        !memoryManager || !gcManager || ! threadpoolManager || !iocpManager) {
       Logger::Critical("Unable to allocate Host Managers");
    }

    m_pTaskManager->AddRef();
    m_pSyncManager->AddRef();
    memoryManager->AddRef();
    gcManager->AddRef();
    threadpoolManager->AddRef();
    iocpManager->AddRef();

    m_pContext->SetTaskManager(m_pTaskManager);
    m_pContext->SetSyncManager(m_pSyncManager);

}

DHHostControl::~DHHostControl()
{
    delete m_pContext;
    m_pRuntimeHost->Release();
    m_pRuntimeControl->Release();
    m_pTaskManager->Release();
    m_pSyncManager->Release();
    memoryManager->Release();
    gcManager->Release();
    threadpoolManager->Release();
    iocpManager->Release();
}

STDMETHODIMP_(VOID) DHHostControl::ShuttingDown()
{
#if LOCK_TRACE
    std::map<SIZE_T, std::vector<SIZE_T>*> *pTrace = m_pContext->GetLockTrace();

    fprintf(stdout, "------------------------------\r\n");
    fprintf(stdout, "Dumping lock trace information\r\n");
    fprintf(stdout, "------------------------------\r\n");
    for (std::map<SIZE_T, std::vector<SIZE_T>*>::iterator trace = pTrace->begin();
        trace != pTrace->end();
        trace++)
    {
        fprintf(stdout, "LOCK %d: ", trace->first);
        for (std::vector<SIZE_T>::iterator innerTrace = trace->second->begin();
            innerTrace != trace->second->end();
            innerTrace++)
        {
            fprintf(stdout, " %d ", *innerTrace);
        }
        fprintf(stdout, "\r\n");
        delete trace->second;
    }

#endif
}

// IUnknown functions

STDMETHODIMP_(DWORD) DHHostControl::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(DWORD) DHHostControl::Release()
{
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0)
        delete this;
    return cRef;
}

STDMETHODIMP DHHostControl::QueryInterface(const IID &riid, void **ppvObject)
{
    if (riid == IID_IUnknown || riid == IID_IHostControl)
    {
        *ppvObject = this;
        AddRef();
        return S_OK;
    }

    *ppvObject = NULL;
    return E_NOINTERFACE;
}

// IHostControl functions

STDMETHODIMP DHHostControl::GetHostManager(const IID &riid, void **ppvHostManager)
{
    if (riid == IID_IHostTaskManager)
        m_pTaskManager->QueryInterface(IID_IHostTaskManager, ppvHostManager);
    else if (riid == IID_IHostSyncManager)
        m_pSyncManager->QueryInterface(IID_IHostSyncManager, ppvHostManager);
    else if (riid == IID_IHostMemoryManager)
       memoryManager->QueryInterface(IID_IHostMemoryManager, ppvHostManager);
    else if (riid == IID_IHostGCManager)
       gcManager->QueryInterface(IID_IHostGCManager, ppvHostManager);
    else if (riid == IID_IHostIoCompletionManager)
       iocpManager->QueryInterface(IID_IHostIoCompletionManager, ppvHostManager);
    else if (riid == IID_IHostThreadpoolManager)
       threadpoolManager->QueryInterface(IID_IHostThreadpoolManager, ppvHostManager);
    else
        ppvHostManager = NULL;

    return S_OK;
}

STDMETHODIMP DHHostControl::SetAppDomainManager(DWORD dwAppDomainID, IUnknown *pUnkAppDomainManager)
{
    _ASSERTE(!"Not implemented");
    return E_NOTIMPL;
}