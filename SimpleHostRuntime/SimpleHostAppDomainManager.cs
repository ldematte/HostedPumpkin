using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security;
using System.Security.Permissions;
using System.Security.Policy;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace SimpleHostRuntime {

   [SecuritySafeCritical]
   [Serializable]
   public enum DomainStatus {
      Ready,
      Busy,      
      Zombie
   };

   [SecuritySafeCritical]
   [Serializable]
   public enum SnippetStatus {
      Success,
      InitializationError,
      Timeout,
      ExecutionError,
      CriticalError,
      ResourceError
   }

   [SecuritySafeCritical]
   [Serializable]
   public struct SnippetResult {
      public SnippetStatus status;
      public Exception exception;
      public long executionTime;
   }

   [ComVisible(true), Guid("59D0437C-7AF2-4CCE-9104-E39970D739D6")]
   public enum HostEventType {
      None = 0,
      OutOfTasks = 1,
      OutOfMemory = 2
   }

   [ComVisible(true), Guid("057732A2-6120-40B9-A65E-9B045A1C0CBB")]
   public struct HostEvent {
      public int eventType; //HostEventType
      public int appDomainId;
      public int managedThreadId;
   }

   [ComVisible(true), Guid("2AF95991-AF3E-4192-B1AC-8FD254E087F3")]
   [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
   public interface IHostContext {
      int GetThreadCount(int appDomainId);
      int GetMemoryUsage(int appDomainId);
      int GetNumberOfZombies();
      void ResetCountersForAppDomain(int appDomainId);
      void UnloadDomain(int appDomainId);

      bool GetLastMessage(int millisecondsTimeout, out HostEvent hostEvent);
   }

   [ComVisible(true), Guid("A603EC84-3449-47B9-BCF5-391C628067D6")]
   [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
   public interface ISimpleHostDomainManager {
      int GetMainThreadManagedId();

      void RegisterHostContext(IHostContext hostContext);

      void RunSnippet(string assemblyFileName, string mainTypeName, string methodName);
      void RunTests(string assemblyFileName, string mainTypeName, string methodNamePrefix);

      void OnMainThreadExit(int appDomainId, bool cleanExit);
   }

   [ComVisible(true), Guid("3D4364E5-790F-4F34-A655-EFB05A40BA07"),
        ClassInterface(ClassInterfaceType.None),
        ComSourceInterfaces(typeof(ISimpleHostDomainManager))]
   [SecuritySafeCritical]
   public class SimpleHostAppDomainManager : System.AppDomainManager, ISimpleHostDomainManager {

      private int mainThreadManagedId = 0;
      public int GetMainThreadManagedId() {
         return mainThreadManagedId;
      }

      static IHostContext hostContext = null;
      static DomainPool domainPool = null;
      public event Action<int> DomainUnload;
      public event Action<int, Exception> FirstChanceException;
      public event Action<int, Object> UnhandledException;
 
      public override void InitializeNewDomain(AppDomainSetup appDomainInfo) {

         // From MSDN "The default InitializeNewDomain implementation does nothing"
         // base.InitializeNewDomain(appDomainInfo);
         InitializationFlags = AppDomainManagerInitializationOptions.RegisterWithHost;

         // Beginning with the .NET Framework 4, you can use this method to sandbox the default 
         // application domain at application startup, or to modify the sandbox of a new application domain. 
         // To do this, adjust the DefaultGrantSet and FullTrustAssemblies properties on the ApplicationTrust 
         // object that is assigned to the AppDomainSetup.ApplicationTrust property of appDomainInfo, before 
         // you initialize the application domain.

         int currentDomainId = AppDomain.CurrentDomain.Id;
         // Hack: call System.Diagnostics.Debug.WriteLine before setting the domain_AssemblyLoad event,
         // or the loading will trigger a loop.
         System.Diagnostics.Debug.WriteLine("Initializing domain " + currentDomainId);


         //if (AppDomain.CurrentDomain.IsDefaultAppDomain()) {

         new ReflectionPermission(PermissionState.Unrestricted).Assert();            

         AppDomain.CurrentDomain.AssemblyResolve += domain_AssemblyResolve;
         AppDomain.CurrentDomain.AssemblyLoad += domain_AssemblyLoad;

         AppDomain.CurrentDomain.DomainUnload += (sender, e) => {
            OnDomainUnload(currentDomainId);
         };
         //AppDomain.CurrentDomain.FirstChanceException += (sender, e) => {
         //   OnFirstChanceException(currentDomainId, e.Exception);
         //};
         AppDomain.CurrentDomain.UnhandledException += (sender, e) => {
            OnUnhandledException(currentDomainId, e.ExceptionObject);
         };

         ReflectionPermission.RevertAssert();
         

         // REVIEW: the assumption, for now, is that the thread that creates the AppDomain is the
         // "main" thread, the one that will execute the snippet. 
         // IF THIS CHANGES, there will be the need to review this bit
         mainThreadManagedId = Thread.CurrentThread.ManagedThreadId;
      }

      void OnDomainUnload(int domainId) {
         System.Diagnostics.Debug.WriteLine("OnDomainUnload: domain " + domainId);
         if (DomainUnload != null)
            DomainUnload(domainId);
      }

      void OnFirstChanceException(int domainId, Exception ex) {
         if (FirstChanceException != null)
            FirstChanceException(domainId, ex);
      }

      void OnUnhandledException(int domainId, Object ex) {
         if (UnhandledException != null)
            UnhandledException(domainId, ex);
      }

      public void RegisterHostContext(IHostContext hostContext) {
         // Call this only for the default appdomain
         Debug.Assert(AppDomain.CurrentDomain.IsDefaultAppDomain());
         // and only once
         Debug.Assert(domainPool == null);
         SimpleHostAppDomainManager.hostContext = hostContext;
         SimpleHostAppDomainManager.domainPool = new DomainPool(this);         
      }     
 
      public void RunSnippet(string assemblyFileName, string mainTypeName, string methodName) {
         domainPool.SubmitSnippet(new SnippetInfo() {
            assemblyFile = System.IO.File.ReadAllBytes(assemblyFileName),
            mainTypeName = mainTypeName, 
            methodName = methodName
         });
      }

      public void RunTests(string assemblyFileName, string mainTypeName, string methodNamePrefix) {

         byte[] rawAssembly = System.IO.File.ReadAllBytes(assemblyFileName);
         var assembly = Assembly.ReflectionOnlyLoad(rawAssembly);

         //Run a single test
         var method = assembly.GetTypes().Where(t => t.Name == mainTypeName || t.FullName == mainTypeName).Single()
               .GetMethods(BindingFlags.Public | BindingFlags.Static)
                  .Where(m => m.Name.StartsWith("SnippetTest12"))
                  .Select(m => m.Name)
                  .Single();

         domainPool.SubmitSnippet(new SnippetInfo() {
            assemblyFile = rawAssembly,
            mainTypeName = mainTypeName,
            methodName = method
         });

         //var methods = assembly.GetType(mainTypeName).GetMethods(BindingFlags.Public | BindingFlags.Static)
         //   .Where(m => m.Name.StartsWith(methodNamePrefix))
         //   .Select(m => m.Name);

         //foreach (var method in methods) {
         //   domainPool.SubmitSnippet(new SnippetInfo() {
         //      assemblyFile = rawAssembly,
         //      mainTypeName = mainTypeName,
         //      methodName = method
         //   });
         //}
      }

      internal bool GetHostMessage(int millisecondsTimeout, out HostEvent hostEvent) {
         return hostContext.GetLastMessage(millisecondsTimeout, out hostEvent);
      }

      public void OnMainThreadExit(int appDomainId, bool cleanExit) {
         // This is one alternative: we let the main thread exit, and we check if it exited cleanly.
         // This puts (asks) more control in the hands of the Host
         // Currently, we adopt another approach: we "return" from the snippet code, but do not exit the thread.
         // So we need a way to check (ask) the host if the AppDomain has more threads running.
         // If so, we need to unload the domain to clean up what was left behind

         System.Diagnostics.Debug.WriteLine("Main thread exited from domain {0}, clean: {1}", appDomainId, cleanExit);
      }

      static Assembly domain_AssemblyResolve(object sender, ResolveEventArgs args) {

         // TODO: Here we have a list of the assemblies we allow in the sandbox.
         // They may be stored in a local dir, or in a DB. Locate them, deserialize (if needed)
         // and load

         var domainManagerAssembly = typeof(SimpleHostAppDomainManager).Assembly;
         if (args.Name.Equals(domainManagerAssembly.FullName)) {

            // NOTE: if you want to load it from DB or file, you will need to asser permission here
            // or the load will fail (no permissions in this sandbox). At least, you need System.Security.Permissions and 
            // System.Security.Permissions.FileIOPermission
            // Assembly.LoadFrom(monitorAssembly.Location);
            return domainManagerAssembly;
         }

         return null;
      }

      private void domain_AssemblyLoad(object sender, AssemblyLoadEventArgs args) {
         System.Diagnostics.Debug.WriteLine("Loaded: " + args.LoadedAssembly.FullName);
      }

      // This kind of customization (provide a custom HostSecurityManager, with custom DomainPolicy etc.)
      // is obsolete in .NET4. Setting the PermissionSet on the AppDomain (as we do here) is the right thing to do
      // See http://msdn.microsoft.com/en-us/library/ee191568(VS.100).aspx
      // and http://msdn.microsoft.com/en-us/library/bb763046(v=vs.100).aspx
      //public override HostSecurityManager HostSecurityManager {
      //   get { return new SimpleHostSecurityManager(); }
      //}

      //[SecuritySafeCritical]
      internal SnippetResult InternalRun(AppDomain appDomain, byte[] assembly, string mainTypeName, string methodName,
                                         bool runningInSandbox) {

         // Here we already are on the new domain
         SnippetResult result = new SnippetResult();
         try {
            // Use this "trick" to go through the standard loader path.
            // This MAY be something we want, or something to avoid

            // When loading the assembly, we need at least FileIOPermission. 
            // Calling it with a full-trust stack. TODO: only what is needed
            (new PermissionSet(PermissionState.Unrestricted)).Assert();
            var clientAssembly = appDomain.Load(assembly);
            //var clientAssembly = Assembly.Load(assembly, null, SecurityContextSource.CurrentAppDomain);
            //AssemblyName an = AssemblyName.GetAssemblyName(assemblyFileName);
            //var clientAssembly = appDomain.Load(an);            
            CodeAccessPermission.RevertAssert();

            //Load the MethodInfo for a method in the new Assembly. This might be a method you know, or 
            //you can use Assembly.EntryPoint to get to the main function in an executable.
            var type = clientAssembly.GetType(mainTypeName);
            if (type == null) {
               result.status = SnippetStatus.InitializationError;
               return result;
            }

            // To allow code to invoke any nonpublic member: Your code must be granted ReflectionPermission with the ReflectionPermissionFlag.MemberAccess flag
            // To allow code to invoke any nonpublic member, as long as the grant set of the assembly that contains the invoked member is 
            // the same as, or a subset of, the grant set of the assembly that contains the invoking code: 
            // Your code must be granted ReflectionPermission with the ReflectionPermissionFlag.RestrictedMemberAccess flag.
            // See http://msdn.microsoft.com/en-us/library/stfy7tfc%28v=vs.110%29.aspx
            var bindingFlags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static;
            if (runningInSandbox)
               bindingFlags = BindingFlags.Public | BindingFlags.Static;

            var target = type.GetMethod(methodName, bindingFlags);
            if (target == null) {
               result.status = SnippetStatus.InitializationError;
               return result;
            }

            //target.S Attributes = target.Attributes ^ MethodAttributes.Private;

            //Now invoke the method.
            target.Invoke(null, null);
            result.status = SnippetStatus.Success;
         }
         catch (TargetInvocationException ex) {
            result.exception = (ex.InnerException == null ? ex : ex.InnerException);

            // When we print informations from a SecurityException extra information can be printed if we are 
            //calling it with a full-trust stack.                  
            (new PermissionSet(PermissionState.Unrestricted)).Assert();
            System.Diagnostics.Debug.WriteLine("Exception caught: =================" +
                                               result.exception.ToString() +
                                               "===================================");
            CodeAccessPermission.RevertAssert();
            
            result.status = SnippetStatus.ExecutionError;
         }
         catch (Exception ex) {
            (new PermissionSet(PermissionState.Unrestricted)).Assert();
            System.Diagnostics.Debug.WriteLine("Exception caught: =================" +
                                               ex.ToString() + 
                                               "===================================");
            CodeAccessPermission.RevertAssert();
            result.exception = ex;
            result.status = SnippetStatus.ExecutionError;
         }
         return result;
      }

      internal int GetThreadCount(int appDomainId) {
         return hostContext.GetThreadCount(appDomainId);
      }

      internal int GetNumberOfZombies() {
         return hostContext.GetNumberOfZombies();
      }

      internal void ResetContextFor(PooledDomainData domainData) {
         domainData.isAborting = 0;
         hostContext.ResetCountersForAppDomain(domainData.domainId);
      }

      internal int GetMemoryUsage(int appDomainId) {
         return hostContext.GetMemoryUsage(appDomainId);
      }

      internal void HostUnloadDomain(int appDomainId) {
         hostContext.UnloadDomain(appDomainId);
      }
   }
}
