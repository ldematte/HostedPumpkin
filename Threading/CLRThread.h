
#ifndef CLR_THREAD_H_INCLUDED
#define CLR_THREAD_H_INCLUDED

#include "../Common.h"

// https://github.com/gbarnett/shared-source-cli-2.0/blob/master/clr/src/vm/threads.h#L412

// WARNING WARNING: this is reveresd engineered and based on the SSCLI
// it is extremely implementation dependent, se we AVOID to use it for anything that is NOT
// a check.

class SHTask;

class CLRThread : public ICLRTask {
public:
   volatile ThreadState m_State; 

   volatile ULONG m_fPreemptiveGCDisabled;
   /*PTR_Frame*/ void* m_pFrame; // Current Frame
   /*PTR_Frame*/ void* m_pUnloadBoundaryFrame;
   /*CLRRootBase* */ void* m_pRoot;
   // Number of locks held by the current thread.
   DWORD m_dwLockCount;
   // The managed thread Id
   DWORD m_ThreadId;
};

#endif //CLR_THREAD_H_INCLUDED

