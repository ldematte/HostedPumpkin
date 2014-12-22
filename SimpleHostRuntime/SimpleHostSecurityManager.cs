using System;
using System.Collections.Generic;
using System.Linq;
using System.Security;
using System.Text;
using System.Threading.Tasks;

namespace SimpleHostRuntime {
   public class SimpleHostSecurityManager : HostSecurityManager {

      // Tell the CLR which customizations we want to partecipate in
      public override HostSecurityManagerOptions Flags {
         get {
            System.Diagnostics.Debug.WriteLine("In HostSecurityManager::Flags");
            return HostSecurityManagerOptions.None;
         }
      }
   }
}
