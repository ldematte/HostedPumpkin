using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Pumpkin;
using System.IO;
using System.Collections.Generic;
using System.Linq;

namespace PumpkinTests {
    [TestClass]
    public class CompilationTests {
        [TestMethod]
        public void CSharpProviderAssembliesShouldBeTransparent() {

            var snippetSource = File.ReadAllText(@"..\..\Tests\FileRead.cs");
            var snippetAssembly = Pumpkin.SnippetCompiler.CompileWithCSC(snippetSource, Path.GetTempPath());

            var assembly = System.Reflection.Assembly.Load(snippetAssembly.assemblyBytes);
            Assert.IsTrue(assembly.GetType("Snippets.FileRead").GetMethod(SnippetCompiler.SnippetMainMethodName).IsSecurityTransparent);
        }

        [TestMethod]
        public void CecilWhitelistAssembly() {
            var snippetSource = File.ReadAllText(@"..\..\Tests\CreateObjectWithActivator.cs");
            var snippetAssembly = Pumpkin.SnippetCompiler.CompileWithCSC(snippetSource, Path.GetTempPath());

            var whiteList = new List<ListEntry> { new ListEntry("mscorlib, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089") };

            Assert.IsTrue(SnippetCompiler.CheckAssemblyAgainstWhitelist(snippetAssembly.assemblyBytes, whiteList));
        }

        [TestMethod]
        public void CecilWhitelistTypes() {
            var snippetSource = File.ReadAllText(@"..\..\Tests\CreateObjectWithActivator.cs");
            var snippetAssembly = Pumpkin.SnippetCompiler.CompileWithCSC(snippetSource, Path.GetTempPath());

            var whiteList = new List<ListEntry> { 
                new ListEntry("mscorlib, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089", "System.Type"),
                new ListEntry("mscorlib, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089", "System.Object"),
                new ListEntry("mscorlib, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089", "System.Threading.Thread"),
                new ListEntry("mscorlib, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089", "System.Activator")
            };

            Assert.IsTrue(SnippetCompiler.CheckAssemblyAgainstWhitelist(snippetAssembly.assemblyBytes, whiteList));
        }

        [TestMethod]
        public void CecilWhitelistMethods() {
            var snippetSource = File.ReadAllText(@"..\..\Tests\CreateObjectWithActivator.cs");
            var snippetAssembly = Pumpkin.SnippetCompiler.CompileWithCSC(snippetSource, Path.GetTempPath());

            var whiteList = new List<ListEntry> { 
                new ListEntry("mscorlib, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089", "System.Type"),
                new ListEntry("mscorlib, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089", "System.Object"),
                new ListEntry("mscorlib, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089", "System.Threading.Thread", "Start"),
                new ListEntry("mscorlib, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089", "System.Activator")
            };

            Assert.IsTrue(SnippetCompiler.CheckAssemblyAgainstWhitelist(snippetAssembly.assemblyBytes, whiteList));
        }

        [TestMethod]
        public void CecilWhitelistMethodsNotListed() {
            var snippetSource = File.ReadAllText(@"..\..\Tests\CreateObjectWithActivator.cs");
            var snippetAssembly = Pumpkin.SnippetCompiler.CompileWithCSC(snippetSource, Path.GetTempPath());

            var whiteList = new List<ListEntry> { 
                new ListEntry("mscorlib, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089", "System.Type"),
                new ListEntry("mscorlib, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089", "System.Object"),
                new ListEntry("mscorlib, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089", "System.Threading.Thread", "Abort"),
                new ListEntry("mscorlib, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089", "System.Activator")
            };

            Assert.IsFalse(SnippetCompiler.CheckAssemblyAgainstWhitelist(snippetAssembly.assemblyBytes, whiteList));
        }
    }
}
