
#ifndef CRST_LOCK_INCLUDED
#define CRST_LOCK_INCLUDED

#include "Common.h"

class CrstLock
{
private:
    BOOL m_bTaken;
    LPCRITICAL_SECTION m_pCrst;
public:
    CrstLock(LPCRITICAL_SECTION pCrst)
    {
        m_pCrst = pCrst;
        m_bTaken = FALSE;
        EnterCriticalSection(m_pCrst);
        m_bTaken = TRUE;
    };

    ~CrstLock()
    {
        if (m_bTaken)
        {
            //_ASSERTE(!"You might have forgotten to release a lock");
            LeaveCriticalSection(m_pCrst);
        }
        m_pCrst = NULL;
    };

    void Exit()
    {
        LeaveCriticalSection(m_pCrst);
        m_bTaken = FALSE;
    };

};

#endif //CRST_LOCK_INCLUDED
