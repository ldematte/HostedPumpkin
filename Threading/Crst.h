
#ifndef CRITICAL_SECTION_H_INCLUDED
#define CRITICAL_SECTION_H_INCLUDED

#include "../Common.h"

class SHCrst : public IHostCrst {
private:
   volatile LONG m_cRef;
   CRITICAL_SECTION* m_pCrst;
public:
   SHCrst();
   SHCrst(DWORD dwSpinCount);
   ~SHCrst();

   // IUnknown functions
   STDMETHODIMP_(DWORD) AddRef();
   STDMETHODIMP_(DWORD) Release();
   STDMETHODIMP QueryInterface(const IID &riid, void **ppvObject);

   // IHostCrst functions
   STDMETHODIMP Enter(DWORD option);
   STDMETHODIMP Leave();
   STDMETHODIMP TryEnter(DWORD option, BOOL *pbSucceeded);
   STDMETHODIMP SetSpinCount(DWORD dwSpinCount);
};

#endif //CRITICAL_SECTION_H_INCLUDED
