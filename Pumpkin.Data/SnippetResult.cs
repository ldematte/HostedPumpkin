using System;
using System.Collections.Generic;
using System.Linq;
using System.Security;
using System.Text;
using System.Threading.Tasks;

namespace Pumpkin.Data {

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
      public string exception;
      public long executionTime;
      public long totalTime;
      public List<string> output;
   }

}
