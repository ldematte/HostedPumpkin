
#ifndef SH_ASSEMBLY_STORE_H_INCLUDED
#define SH_ASSEMBLY_STORE_H_INCLUDED

#include "../Common.h"

#include <list>
#include "AssemblyInfo.h"

class SHAssemblyStore : public IHostAssemblyStore {

private:
   volatile LONG m_cRef;
   std::list<AssemblyInfo>* hostAssemblies;

   bool SameAssembly(const std::wstring& availableName, LPCWSTR requiredName);

public:
   SHAssemblyStore(std::list<AssemblyInfo>*);
   ~SHAssemblyStore();

   // IUnknown functions
   STDMETHODIMP_(DWORD) AddRef();
   STDMETHODIMP_(DWORD) Release();
   STDMETHODIMP QueryInterface(const IID &riid, void **ppvObject);

   // IAssemblyStore functions
   STDMETHODIMP ProvideAssembly(
      /* [in] */ AssemblyBindInfo *pBindInfo,
      /* [out] */ UINT64 *pAssemblyId,
      /* [out] */ UINT64 *pContext,
      /* [out] */ IStream **ppStmAssemblyImage,
      /* [out] */ IStream **ppStmPDB);

   STDMETHODIMP ProvideModule(
      /* [in] */ ModuleBindInfo *pBindInfo,
      /* [out] */ DWORD *pdwModuleId,
      /* [out] */ IStream **ppStmModuleImage,
      /* [out] */ IStream **ppStmPDB);
};

#endif //SH_ASSEMBLY_STORE_H_INCLUDED

