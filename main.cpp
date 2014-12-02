
#include "Common.h"
#include "Logger.h"

#include "HostCtrl.h"


int _tmain(int argc, _TCHAR* argv [])
{

   HRESULT hr;

   ICLRMetaHost* metaHost = NULL;
   ICLRRuntimeInfo* runtimeInfo = NULL;

   // 
   // Load and start the .NET runtime.
   // 

   _TCHAR* clrVersion = L"v2.0.50727";
   if (argc > 1) {
      clrVersion = argv[0];
   }

   Logger::Info("Load and start the .NET runtime %s", argv[0]);

   hr = CLRCreateInstance(CLSID_CLRMetaHost, IID_PPV_ARGS(&metaHost));
   if (FAILED(hr))
   {
      wprintf(L"CLRCreateInstance failed w/hr 0x%08lx\n", hr);
      return -1;
   }

   // Get the ICLRRuntimeInfo corresponding to a particular CLR version. It 
   // supersedes CorBindToRuntimeEx with STARTUP_LOADER_SAFEMODE.
   hr = metaHost->GetRuntime(clrVersion, IID_PPV_ARGS(&runtimeInfo));
   if (FAILED(hr))
   {
      wprintf(L"ICLRMetaHost::GetRuntime failed w/hr 0x%08lx\n", hr);
      metaHost->Release();
      return -1;
   }

   // Check if the specified runtime can be loaded into the process. This 
   // method will take into account other runtimes that may already be 
   // loaded into the process and set pbLoadable to TRUE if this runtime can 
   // be loaded in an in-process side-by-side fashion. 
   BOOL isLoadable;
   hr = runtimeInfo->IsLoadable(&isLoadable);
   if (FAILED(hr))
   {
      wprintf(L"ICLRRuntimeInfo::IsLoadable failed w/hr 0x%08lx\n", hr);
      runtimeInfo->Release();
      metaHost->Release();
      return -1;
   }

   if (!isLoadable)
   {
      wprintf(L".NET runtime %s cannot be loaded\n", clrVersion);
      runtimeInfo->Release();
      metaHost->Release();
      return -1;
   }

   // Load the CLR into the current process and return a runtime interface 
   // pointer. ICorRuntimeHost and ICLRRuntimeHost are the two CLR hosting  
   // interfaces supported by CLR 4.0. 
   // The ICorRuntimeHost interface that was provided in .NET v1.x, is compatible with all 
   // .NET Frameworks. 

   //ICorRuntimeHost *corRuntimeHost = NULL;
   //hr = runtimeInfo->GetInterface(CLSID_CorRuntimeHost,
   //	IID_PPV_ARGS(&corRuntimeHost));

   ICLRRuntimeHost* clrRuntimeHost = NULL;
   hr = runtimeInfo->GetInterface(CLSID_CLRRuntimeHost,
      IID_PPV_ARGS(&clrRuntimeHost));
   if (FAILED(hr))
   {
      wprintf(L"ICLRRuntimeInfo::GetInterface failed w/hr 0x%08lx\n", hr);
      runtimeInfo->Release();
      metaHost->Release();
      return -1;
   }

   // Start the CLR.
   hr = clrRuntimeHost->Start();
   if (FAILED(hr))
   {
      wprintf(L"CLR failed to start w/hr 0x%08lx\n", hr);
      runtimeInfo->Release();
      metaHost->Release();
      clrRuntimeHost->Release();
      return -1;
   }

   // Construct our host control object.
   DHHostControl* hostControl = new DHHostControl(clrRuntimeHost);
   clrRuntimeHost->SetHostControl(hostControl);

   // Now, begin the CLR.
   hr = clrRuntimeHost->Start();
   if (hr == S_FALSE)
      wprintf(L"Runtime already started; probably OK to proceed");
   else {
      wprintf(L"Runtime startup failed (0x%x)", hr);
      runtimeInfo->Release();
      metaHost->Release();
      clrRuntimeHost->Release();
      return -1;
   }

   // Construct the shim path
   WCHAR wcShimPath[MAX_PATH];
   if (!GetCurrentDirectoryW(MAX_PATH, wcShimPath)) {
      wprintf(L"GetCurrentDirectory failed (0x%x)", HRESULT_FROM_WIN32(GetLastError()));
      runtimeInfo->Release();
      metaHost->Release();
      clrRuntimeHost->Release();
      return -1;
   }

   wcsncat_s(wcShimPath, sizeof(wcShimPath) / sizeof(WCHAR), L"\\shim.exe", MAX_PATH - wcslen(wcShimPath) - 1);

   // Gather the arguments to pass to the shim.
   LPWSTR wcShimArgs = NULL;
   if (argc > 1)
   {
      SIZE_T totalLength = 1; // 1 is the NULL terminator
      for (int i = 1; i < argc; i++)
      {
         // TODO: add characters for quotes around args w/ spaces inside them
         if (i != 1)
            totalLength++; // add a space between args
         totalLength += _tcslen(argv[i]) + 1;
      }

      wcShimArgs = new WCHAR[totalLength];
      wcShimArgs[0] = '\0';

      for (int i = 1; i < argc; i++)
      {
         if (i != 1)
            wcscat_s(wcShimArgs, totalLength, L" ");
         wcsncat_s(wcShimArgs, totalLength, argv[i], wcslen(argv[i]));
      }
   }

   if (wcShimArgs == NULL)
      Logger::Critical("Missing program path (host.exe <exePath>)");

   // And execute the program...
   DWORD retVal;
   HRESULT hrExecute = clrRuntimeHost->ExecuteInDefaultAppDomain(
      wcShimPath,
      L"Shim",
      L"Start",
      wcShimArgs,
      &retVal);

   if (!SUCCEEDED(hrExecute))
      Logger::Critical("Execution of shim failed: 0x%x", hrExecute);

   if (wcShimArgs)
      delete wcShimArgs;

   // Stop the CLR and cleanup.
   hostControl->ShuttingDown();
   clrRuntimeHost->Stop();
   clrRuntimeHost->Release();

   return retVal;
}
