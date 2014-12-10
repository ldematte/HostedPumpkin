using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace SimpleHostRuntime {

   [ComVisible(true), Guid("A603EC84-3449-47B9-BCF5-391C628067D6")]
   [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
   public interface ISimpleHostDomainManager {

      int Run(string assemblyFileName);
      int Run(string assemblyFileName, string mainTypeName, string methodName);

   }

   [ComVisible(true), Guid("3D4364E5-790F-4F34-A655-EFB05A40BA07"),
        ClassInterface(ClassInterfaceType.None),
        ComSourceInterfaces(typeof(ISimpleHostDomainManager))]
   public class SimpleHostAppDomainManager : System.AppDomainManager, ISimpleHostDomainManager {
   
      public override void InitializeNewDomain(AppDomainSetup appDomainInfo) {
         //base.InitializeNewDomain(appDomainInfo);
         InitializationFlags = AppDomainManagerInitializationOptions.RegisterWithHost;
      }

      public int Run(string assemblyFileName) {
         var appDomain = AppDomain.CreateDomain("Host Domain");
         int exitCode = appDomain.ExecuteAssembly(assemblyFileName);
         AppDomain.Unload(appDomain);
         return exitCode;
      }

      public int Run(string assemblyFileName, string mainTypeName, string methodName) {
         throw new NotImplementedException();
      }
   }
}
