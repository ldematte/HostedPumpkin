using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace Pumpkin.Tests {

   [TestClass]
   public class ExecutionTests {

      [TestMethod]
      public void AsyncAwaitDeepPatch() {
         var snippetSource = File.ReadAllText(@"..\..\Tests\AsyncAwait.cs");
         var compiledSnippet = Pumpkin.SnippetCompiler.CompileWithCSC(snippetSource, Path.GetTempPath());

         Assert.IsTrue(compiledSnippet.success);
         var patchedAssemblyBytes = Pumpkin.SnippetCompiler.PatchAssembly(compiledSnippet.assemblyBytes, "Snippets.AsyncAwait");
         File.WriteAllBytes(Path.Combine(Path.GetTempPath(), "AsyncAwait-patched.dll"), patchedAssemblyBytes);

         var clientAssembly = AppDomain.CurrentDomain.Load(patchedAssemblyBytes);

         var type = clientAssembly.GetTypes().Where(t => t.FullName == "Snippets.AsyncAwait").SingleOrDefault();
         Assert.IsNotNull(type);


         var monitorField = type.GetField(Pumpkin.Monitor.MonitorFieldName, BindingFlags.Public | BindingFlags.Static);         
         Assert.IsNotNull(monitorField);
         
         var resultOutput = new List<string>();
         var monitor = new Pumpkin.Monitor(resultOutput);
         monitorField.SetValue(null, monitor);
         

         var bindingFlags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static;
         var target = type.GetMethod("SnippetMain", bindingFlags);
         
         Assert.IsNotNull(target);           

         //Now invoke the method.
         target.Invoke(null, null);

         Assert.AreEqual(resultOutput[0], "Hello Foo");
      }
   }
}
