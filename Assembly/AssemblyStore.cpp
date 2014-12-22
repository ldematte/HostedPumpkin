

#include "AssemblyStore.h"
#include "FileStream.h"
#include "../Logger.h"

SHAssemblyStore::SHAssemblyStore(std::list<AssemblyInfo>* assemblies) {
   hostAssemblies = assemblies;
   m_cRef = 0;
}

SHAssemblyStore::~SHAssemblyStore() {
}


HRESULT CreateStreamFromFile(LPCWSTR fileName, IStream **ppStream) {
   // That's ... unusual: there is no straightforward, supported way to do that in COM.
   // We may implement it ourservels, or use a "surrogate"
   // There is "SHCreateStreamOnFile", which is implemented by the Shell. Not ideal.
   // There is "URLOpenBlockingStream" from IE, which is implemented in Urlmon. Maybe better
   SHFileStream* fileStream = new SHFileStream(fileName);
   HRESULT hr = fileStream->OpenRead();
   if (SUCCEEDED(hr)) {
      *ppStream = fileStream;
   }
   return hr;
}

bool SHAssemblyStore::SameAssembly(const std::wstring& availableName, LPCWSTR requiredName) {
   size_t requiredNameLen = wcslen(requiredName);
   return requiredNameLen == availableName.size() &&
      (_wcsnicmp(availableName.c_str(), requiredName, availableName.size()) == 0);
}

// IHostAssemblyStore
HRESULT STDMETHODCALLTYPE SHAssemblyStore::ProvideAssembly(
   AssemblyBindInfo *pBindInfo,
   UINT64           *pAssemblyId,
   UINT64           *pContext,
   IStream          **ppStmAssemblyImage,
   IStream          **ppStmPDB) {
   
   Logger::Debug(L"ProvideAssembly called for binding identity '%s' in domain %d ", pBindInfo->lpPostPolicyIdentity, pBindInfo->dwAppDomainId);

   // Change the assembly probing mechanism. See
   // http://blogs.msdn.com/b/junfeng/archive/2006/03/27/561775.aspx

   // Check to see if Administrator policy was applied.  If so, print an error
   // to the command line and return "file not found".  This will cause the 
   // execution to continue searching along the normal path
   if (pBindInfo->ePolicyLevel == ePolicyLevelAdmin) {
      Logger::Debug(L"Administrator Version Policy is present that redirects: %s to %s. Stopping search", pBindInfo->lpReferencedIdentity, pBindInfo->lpPostPolicyIdentity);
      return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
   }


   // We don't use pContext for any host-specific data - set it to 0. 
   *pContext = 0;

   // WARNING WARNING! This cannot be "0" or the CLR will freak out (and will start to search for types in the wrong assemblies)
   UINT64 id = 10000; 
   std::list<AssemblyInfo>::iterator it;
   for (it = hostAssemblies->begin(); it != hostAssemblies->end(); ++it, ++id) {
      if (SameAssembly(it->FullName, pBindInfo->lpPostPolicyIdentity)) {
         *pAssemblyId = id;

         if (it->AssemblyLoadPath.empty()) {
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
         }

         HRESULT hr = CreateStreamFromFile(it->AssemblyLoadPath.c_str(), ppStmAssemblyImage);
         if (FAILED(hr)) {
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
         }

         if (!it->AssemblyDebugInfoPath.empty()) {
            hr = CreateStreamFromFile(it->AssemblyDebugInfoPath.c_str(), ppStmPDB);
            if (FAILED(hr)) {
               // Just skip debug info, if not found
               Logger::Error(L"Cannot load debug info from %s", it->AssemblyDebugInfoPath);
               *ppStmPDB = NULL;
            }         
         }

         Logger::Debug("Assembly provided by HOST storage");
         return S_OK;
      }      
   }

   // not something we are interested in?
   // Ask the CLR to continue with the standart app base probe
   // TODO: simply fail and stop?
   return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND); 
}


HRESULT STDMETHODCALLTYPE SHAssemblyStore::ProvideModule(
   ModuleBindInfo* pBindInfo,
   DWORD* /*pdwModuleId*/,
   IStream** /*ppStmModuleImage*/,
   IStream** /*ppStmPDB*/) {

   Logger::Debug(L"ProvideModule called for binding identity '%s' in domain %d", pBindInfo->lpAssemblyIdentity, pBindInfo->dwAppDomainId);

   // Let the CLR load every module
   return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
}

//
// IUnknown
//
HRESULT STDMETHODCALLTYPE SHAssemblyStore::QueryInterface(const IID &riid, void **ppv) {
   if (!ppv) 
      return E_POINTER;

   if (riid == IID_IUnknown || riid == IID_IHostAssemblyStore) {
      *ppv = this;
      AddRef();
      return S_OK;
   }

   *ppv = NULL;
   return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE SHAssemblyStore::AddRef() {
   return InterlockedIncrement(&m_cRef);
}

ULONG STDMETHODCALLTYPE SHAssemblyStore::Release() {
   if (InterlockedDecrement(&m_cRef) == 0) {
      delete this;
      return 0;
   }
   return m_cRef;
}
