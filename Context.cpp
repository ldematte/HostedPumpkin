
#include "Context.h"

#include "Threading\Task.h"
#include "Threading\TaskMgr.h"
#include "Threading\SyncMgr.h"

#include "CrstLock.h"

DHContext::DHContext()
{
    m_pTaskManager = NULL;
    m_pSyncManager = NULL;

    // Allocate 
    m_pLockWaits = new std::map<IHostTask*, SIZE_T>;
    m_pCrst = new CRITICAL_SECTION;
    if (!m_pLockWaits || !m_pCrst)
    {
        fprintf(stderr, "Error allocating deadlock context");
        exit(-1);
    }

    InitializeCriticalSection(m_pCrst);

#if LOCK_TRACE
    m_dwTlsCurrentLocks = TlsAlloc();
    m_pLockTrace = new std::map<SIZE_T, std::vector<SIZE_T>*>;
    m_pLockTraceCrst = new CRITICAL_SECTION;
    if (!m_pLockTrace || !m_pLockTraceCrst)
    {
        fprintf(stderr, "Error allocating deadlock context");
        exit(-1);
    }

    InitializeCriticalSection(m_pLockTraceCrst);
#endif
}

DHContext::~DHContext()
{
    if (m_pTaskManager) m_pTaskManager->Release();
    if (m_pSyncManager) m_pSyncManager->Release();
    if (m_pLockWaits) delete m_pLockWaits;
    if (m_pCrst) DeleteCriticalSection(m_pCrst);
    
#if LOCK_TRACE
    if (m_pLockTrace) delete m_pLockTrace;
    if (m_pLockTraceCrst) DeleteCriticalSection(m_pLockTraceCrst);
#endif
}

STDMETHODIMP_(VOID) DHContext::SetTaskManager(SHTaskManager *pTaskManager)
{
    pTaskManager->AddRef();
    m_pTaskManager = pTaskManager;
}

STDMETHODIMP_(VOID) DHContext::SetSyncManager(SHSyncManager *pSyncManager)
{
    pSyncManager->AddRef();
    m_pSyncManager = pSyncManager;
}

HRESULT DHContext::HostWait(HANDLE hWait, DWORD dwMilliseconds, DWORD dwOption)
{
    DWORD dwWaitResult;
    BOOL bAlertable = dwOption & WAIT_ALERTABLE;

    if (dwOption & WAIT_MSGPUMP)
    {
        dwWaitResult = CoWaitForMultipleHandles(
            bAlertable ? COWAIT_ALERTABLE : 0, dwMilliseconds, 1, &hWait, NULL);
    }
    else
    {
        dwWaitResult = WaitForSingleObjectEx(hWait, dwMilliseconds, bAlertable);
    }

    switch (dwWaitResult)
    {
        case WAIT_OBJECT_0:
            return S_OK;
        case WAIT_ABANDONED:
            return HOST_E_ABANDONED;
        case WAIT_IO_COMPLETION:
            return HOST_E_INTERRUPTED;
        case WAIT_TIMEOUT:
            return HOST_E_TIMEOUT;
        case WAIT_FAILED:
            _ASSERTE(!"*WaitForSingleObjectEx failed");
            return HRESULT_FROM_WIN32(GetLastError());
        default:
            _ASSERTE(!"unexpected return value");
            return E_FAIL;
    }
}

void DHContext::PrintDeadlock(DHDetails *pDetails, SIZE_T cookie)
{
    ICLRSyncManager *pClrSyncManager = m_pSyncManager->GetCLRSyncManager();

    fprintf(stderr, "A deadlock has occurred; terminating the program. Details below.\r\n");

    // Print details about the attempted acquisition:
    IHostTask *pTask;
    m_pTaskManager->GetCurrentTask((IHostTask**)&pTask);
    fprintf(stderr, "  %x was attempting to acquire %x, which created the cycle\r\n",
        dynamic_cast<SHTask*>(pTask)->GetThreadHandle(), cookie);
    pTask->Release();

    // Now walk through the wait-graph and print details:
    for (DHDetails::iterator walker = pDetails->begin();
        walker != pDetails->end();
        walker++)
    {
        IHostTask *pOwner;
        pClrSyncManager->GetMonitorOwner(walker->second, (IHostTask**)&pOwner);        

        fprintf(stderr, "  %x waits on lock %x (owned by %x)\r\n",
            walker->first->GetThreadHandle(), walker->second,
            dynamic_cast<SHTask*>(pOwner)->GetThreadHandle());

        pOwner->Release();
        walker->first->Release();
    }
}

STDMETHODIMP_(DHDetails*) DHContext::TryEnter(SIZE_T cookie)
{
    IHostTask* pCurrentTask;
    m_pTaskManager->GetCurrentTask((IHostTask**)&pCurrentTask);
    
    // When we "try-enter," we simply note that this thread is entering a wait
    // for the target lock. This is done under the CRST because multiple threads
    // can try to insert a record into the shared list.
    CrstLock lock(m_pCrst);
    m_pLockWaits->insert(std::map<IHostTask*, SIZE_T>::value_type(pCurrentTask, cookie));
    lock.Exit();

    // Now we detect whether a deadlock has occurred.
    return DetectDeadlock(cookie);
}

STDMETHODIMP_(VOID) DHContext::EndEnter(SIZE_T cookie)
{
    IHostTask* pCurrentTask;
    m_pTaskManager->GetCurrentTask((IHostTask**)&pCurrentTask);

    // This call is made after the thread has acquired the lock. We simply remove
    // our wait record to indicate that we're no longer waiting for the target lock.
    CrstLock lock(m_pCrst);
    m_pLockWaits->erase(pCurrentTask);
    lock.Exit();

#if LOCK_TRACE
    // If we're tracing, enter a record into the list of currently held locks.
    std::vector<SIZE_T> *pCurrentLocks = reinterpret_cast<std::vector<SIZE_T>*>(TlsGetValue(m_dwTlsCurrentLocks));
    if (!pCurrentLocks)
    {
        pCurrentLocks = new std::vector<SIZE_T>;
        TlsSetValue(m_dwTlsCurrentLocks, pCurrentLocks);
    }
    pCurrentLocks->push_back(cookie);

    // And record the uniquely held locks in the active context.
    CrstLock traceLock(m_pLockTraceCrst);
    for (std::vector<SIZE_T>::iterator lockWalker = pCurrentLocks->begin();
        lockWalker != pCurrentLocks->end();
        lockWalker++)
    {
        // Obtain the list of locks held for the current lock.
        std::map<SIZE_T, std::vector<SIZE_T>*>::iterator traceWalker = m_pLockTrace->find(*lockWalker);
        std::vector<SIZE_T> *pTrace;
        if (traceWalker == m_pLockTrace->end())
        {
         pTrace = new std::vector<SIZE_T>;
         m_pLockTrace->insert(std::map<SIZE_T, std::vector<SIZE_T>*>::value_type(*lockWalker, pTrace));
        }
        else
        {
            pTrace = traceWalker->second;
        }

        // And ensure all locks held from here on are added.
      for (std::vector<SIZE_T>::iterator heldLocks(lockWalker);
            heldLocks != pCurrentLocks->end();
            heldLocks++)
        {            
            pTrace->push_back(*heldLocks);
        }
    }
    traceLock.Exit();
#endif

    // This may look strange. We Release the underlying IHostTask twice. But this is an
    // operation paired with TryEnter above, which required a call to GetCurrentTask, the
    // result for which we stashed in our map. Thus, we need to Release once for TryEnter
    // and once for the call just above in EndEnter.
    pCurrentTask->Release();
    pCurrentTask->Release();
}

STDMETHODIMP_(VOID) DHContext::Release(SIZE_T cookie)
{
#if LOCK_TRACE
    // If we're tracing, enter a record into the list of currently held locks.
   std::vector<SIZE_T> *pCurrentLocks = reinterpret_cast<std::vector<SIZE_T>*>(TlsGetValue(m_dwTlsCurrentLocks));
    if (pCurrentLocks)
    {
        _ASSERTE(!pCurrentLocks->empty() && "Expected a non-empty list");

        if (pCurrentLocks->back() == cookie)
        {
            // This is the fast case (vector is constant time deletion from front/back).
            pCurrentLocks->pop_back();
        }
        else
        {
            // Slow case, we must manually iterate through the list.
         for (std::vector<SIZE_T>::iterator currentWalker = pCurrentLocks->end();
                currentWalker != pCurrentLocks->begin();
                currentWalker--)
            {
                if (*currentWalker == cookie)
                {
                    pCurrentLocks->erase(currentWalker);
                    break;
                }
            }
        }

        // If the current list is empty, remove it from TLS and delete it. We will lazily
        // re-create it if necessary.
        if (pCurrentLocks->empty())
        {
            TlsSetValue(m_dwTlsCurrentLocks, NULL);
            delete pCurrentLocks;
        }
    }
#endif
}

STDMETHODIMP_(DHDetails*) DHContext::DetectDeadlock(SIZE_T cookie)
{
    DHDetails *pCycle = NULL;
    ICLRSyncManager *pClrSyncManager = m_pSyncManager->GetCLRSyncManager();

    IHostTask* pOwner;
    m_pTaskManager->GetCurrentTask((IHostTask**)&pOwner);

    // Construct a wait graph. This is done roughly as follows
    //     - who owns the current lock? if nobody, we're done, no deadlock;
    //     - have we seen that owner in our wait-graph traversal? if yes, deadlock!
    //     - what lock is she waiting on? if nobody, we're done, no deadlock;
    //     - repeat...

   std::vector<IHostTask*> waitGraph;
    waitGraph.push_back(pOwner);

    SIZE_T current = cookie;
    while (TRUE)
    {
        // If the lock is already owned, this will retrieve its owner:
        pClrSyncManager->GetMonitorOwner(current, (IHostTask**)&pOwner);
        if (!pOwner)
            goto funcExit;

        // The lock is owned already. Check the graph for a cycle. If we've ever seen
        // this owner in our search, we are in a deadlock.
        BOOL bCycleFound = FALSE;
      std::vector<IHostTask*>::iterator walker = waitGraph.begin();
        while (walker != waitGraph.end())
        {
            if (*walker == pOwner)
            {
                // We have detected a cycle; from here until the end of the list.
                bCycleFound = TRUE;
                break;
            }

            walker++;
        }

        if (bCycleFound)
        {
            // Construct a list of the cycle information. This is just a list of IHostTask, SIZE_T
            // pairs, listed in order of the wait graph.
            pCycle = new DHDetails();
            CrstLock lock(m_pCrst);
            while (walker != waitGraph.end())
            {
            std::map<IHostTask*, SIZE_T>::iterator waitMatch = m_pLockWaits->find(*walker);
                waitMatch->first->AddRef();
                pCycle->insert(DHDetails::value_type(
                    dynamic_cast<SHTask*>(waitMatch->first), waitMatch->second));
                walker++;
            }
            lock.Exit();

            goto funcExit;
        }

        waitGraph.push_back(pOwner);

        // No cycle was found. Try to determine whether the owner is likewise waiting on
        // somebody. If they are, we continue our deadlock-detection algorithm.
        CrstLock lock(m_pCrst);
      std::map<IHostTask*, SIZE_T>::iterator waitMatch = m_pLockWaits->find(pOwner);
        if (waitMatch == m_pLockWaits->end())
            break;
        current = waitMatch->second;
        lock.Exit();
    }

funcExit:
    // IHostTasks are AddRef'd when we call GetMonitorOwner. Thus, any of them must be
    // released before continuing on with our miserable lives.
   for (std::vector<IHostTask*>::iterator walker = waitGraph.begin();
        walker != waitGraph.end();
        walker++)
    {
        (*walker)->Release();
    }

    pClrSyncManager->Release();

    return pCycle;
}
