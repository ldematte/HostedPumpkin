
#ifndef SH_ASSEMBLY_MANAGER_H_INCLUDED
#define SH_ASSEMBLY_MANAGER_H_INCLUDED

#include "../Common.h"

#include <list>
#include "AssemblyInfo.h"

class SHAssemblyManager : public IHostAssemblyManager {

private:
   volatile LONG m_cRef;
   IHostAssemblyStore* assemblyStore;
   std::list<AssemblyInfo> m_hostAssemblies;

public:
   SHAssemblyManager(const std::list<AssemblyInfo>&);
   ~SHAssemblyManager();

   // IUnknown functions
   STDMETHODIMP_(DWORD) AddRef();
   STDMETHODIMP_(DWORD) Release();
   STDMETHODIMP QueryInterface(const IID &riid, void **ppvObject);

   // IAssemblyManager functions
   STDMETHODIMP GetNonHostStoreAssemblies(/* [out] */ ICLRAssemblyReferenceList **ppReferenceList);
   STDMETHODIMP GetAssemblyStore(/* [out] */ IHostAssemblyStore **ppAssemblyStore);
};

#endif //SH_ASSEMBLY_MANAGER_H_INCLUDED

