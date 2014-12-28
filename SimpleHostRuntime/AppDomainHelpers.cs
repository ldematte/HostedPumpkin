using System;
using System.Collections.Generic;
using System.Linq;
using System.Security;
using System.Security.Permissions;
using System.Security.Policy;
using System.Text;
using System.Threading.Tasks;

namespace SimpleHostRuntime {
   public static class AppDomainHelpers {

      public static AppDomain CreateSandbox(string sandboxName) {

         //Setting the AppDomainSetup. It is very important to set the ApplicationBase to a folder 
         //other than the one in which the sandboxer resides.
         AppDomainSetup adSetup = new AppDomainSetup();
         adSetup.ApplicationBase = "NOT_A_PATH";
         // Do not search the application base directory at all
         adSetup.DisallowApplicationBaseProbing = true;

         // With the restrictions we have this should never work anyway.
         // But let's set it in case future versions allow some sort of network access...
         adSetup.DisallowCodeDownload = true;

         //Setting the permissions for the AppDomain. We give the permission to execute and to 
         //read/discover the location where the untrusted code is loaded.
         PermissionSet permSet = new PermissionSet(PermissionState.None);
         permSet.AddPermission(new SecurityPermission(SecurityPermissionFlag.Execution));

         //We want the sandboxer assembly's strong name, so that we can add it to the full trust list.
         StrongName fullTrustAssembly = typeof(SimpleHostAppDomainManager).Assembly.Evidence.GetHostEvidence<StrongName>();

         //Now we have everything we need to create the AppDomain, so let's create it.
         AppDomain domain = AppDomain.CreateDomain(sandboxName, null, adSetup, permSet, fullTrustAssembly);
         AppDomain.MonitoringIsEnabled = true;

         return domain;
      }
   }
}
