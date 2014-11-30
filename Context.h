
#ifndef CONTEXT_H_INCLUDED
#define CONTEXT_H_INCLUDED

#include "Common.h"

#include <map>
#include <vector>

#include <mscoree.h>
#include <corerror.h>


class DHTask;
class SHTaskManager;
class SHSyncManager;

typedef std::map<DHTask*, SIZE_T> DHDetails;

class DHContext
{
private:
    SHTaskManager *m_pTaskManager;
    SHSyncManager *m_pSyncManager;
    std::map<IHostTask*, SIZE_T> *m_pLockWaits;
    LPCRITICAL_SECTION m_pCrst;

#if LOCK_TRACE
    DWORD m_dwTlsCurrentLocks;
    std::map<SIZE_T, std::vector<SIZE_T>*> *m_pLockTrace;
    LPCRITICAL_SECTION m_pLockTraceCrst;
#endif

public:
    DHContext();
    ~DHContext();

    std::map<SIZE_T, std::vector<SIZE_T>*>* GetLockTrace() { return m_pLockTrace; };
    static HRESULT HostWait(HANDLE hWait, DWORD dwMilliseconds, DWORD dwOption);
    void PrintDeadlock(DHDetails *pDetails, SIZE_T cookie);

    STDMETHODIMP_(VOID) SetTaskManager(SHTaskManager *pTaskManager);
    STDMETHODIMP_(VOID) SetSyncManager(SHSyncManager *pSyncManager);

    STDMETHODIMP_(DHDetails*) TryEnter(SIZE_T cookie);
    STDMETHODIMP_(VOID) EndEnter(SIZE_T cookie);
    STDMETHODIMP_(VOID) Release(SIZE_T cookie);
    STDMETHODIMP_(DHDetails*) DetectDeadlock(SIZE_T cookie);
};

#endif //CONTEXT_H_INCLUDED
