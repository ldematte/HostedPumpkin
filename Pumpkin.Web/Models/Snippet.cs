using System;
using System.Collections.Generic;

namespace Pumpkin.Web.Models {

   public class Snippet {
      public string Id { get; set; }
      public string Usings { get; set; }
      public string Source { get; set; }

      private string compilerVersion;
      public string CompilerVersion { 
         get { return (String.IsNullOrEmpty(compilerVersion) ? "<default>" : compilerVersion); }
         set { compilerVersion = value; }
      }

      private string statusColor = "white";
      public string StatusColor { 
         get { return statusColor; }
         set { statusColor = value; }
      }

      public IEnumerable<String> Errors { get; set; }
   }
}