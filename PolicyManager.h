
#ifndef SH_POLICY_MANAGER_H_INCLUDED
#define SH_POLICY_MANAGER_H_INCLUDED

#include "Common.h"
#include "HostContext.h"

class SHPolicyManager : public IHostPolicyManager {
private:
   volatile LONG m_cRef;
   HostContext* hostContext;

public:
   SHPolicyManager(HostContext* hostContext);
   virtual ~SHPolicyManager();

   // IUnknown functions
   STDMETHODIMP_(DWORD) AddRef();
   STDMETHODIMP_(DWORD) Release();
   STDMETHODIMP QueryInterface(const IID &riid, void **ppvObject);

   // IHostPolicyManager functions
   STDMETHODIMP OnDefaultAction(
      /* [in] */ EClrOperation operation,
      /* [in] */ EPolicyAction action);

   STDMETHODIMP OnTimeout(
      /* [in] */ EClrOperation operation,
      /* [in] */ EPolicyAction action) ;

   STDMETHODIMP OnFailure(
      /* [in] */ EClrFailure failure,
      /* [in] */ EPolicyAction action);

};

#endif //SH_POLICY_MANAGER_H_INCLUDED
