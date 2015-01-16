using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Microsoft.CSharp;
using System.CodeDom.Compiler;
using System.Reflection;
using System.IO;
using Mono.Cecil;
using Mono.Cecil.Cil;

namespace Pumpkin {

   public class SnippetCompilationResult {
      public byte[] assemblyBytes;
      public Guid snippetGuid;
      public bool success;
      public IEnumerable<string> errors;
   }


   public class SnippetCompiler {

      // TODO: a name allowed in the CLR, but disallowed in C#
      public const string MonitorFieldName = "__monitor";
      public const string SnippetMainMethodName = "SnippetMain";

      public static SnippetCompilationResult CompileWithCSC(string snippetSource, string tmpPath) {

         var options = new Dictionary<string, string> { { "CompilerVersion", "v4.0" } };
         var csc = new CSharpCodeProvider(options);

         var snippetGuid = Guid.NewGuid();

         var compilerParams = new CompilerParameters();
         compilerParams.OutputAssembly = Path.Combine(tmpPath, "snippet" + snippetGuid.ToString() + ".dll");

         // Add the assemblies we use in our snippet, implicit or explicit.
         // Our own monitor always goes in
         compilerParams.ReferencedAssemblies.Add(typeof(Pumpkin.Monitor).Assembly.Location);
         compilerParams.ReferencedAssemblies.Add("System.dll");
         compilerParams.ReferencedAssemblies.Add("System.Core.dll");

         var assemblyInfoCs = "[assembly: System.Security.SecurityTransparent]";

         var compilerResults = csc.CompileAssemblyFromSource(compilerParams, snippetSource, assemblyInfoCs);
         // TODO: handle compilation errors

         return new SnippetCompilationResult() { 
            assemblyBytes = File.ReadAllBytes(compilerResults.PathToAssembly),
            snippetGuid = snippetGuid,
            success = !compilerResults.Errors.HasErrors,
            errors = compilerResults.Errors.Cast<CompilerError>().Select(e => e.ErrorText)
         };
      }

      public static bool CheckAssemblyAgainstWhitelist(byte[] assembly, List<ListEntry> whiteList) {

         // Use cecil to inspect this assembly.
         // We check if a class and/or a method is allowed.
         // We allow creation of any object, and call of any method inside the assembly itself

         using (var memoryStream = new MemoryStream(assembly)) {
            var module = ModuleDefinition.ReadModule(memoryStream);

            var ownTypes = module.Types.ToDictionary(type => type.FullName);

            foreach (var type in module.Types) {
               foreach (var method in type.Methods) {
                  var il = method.Body.GetILProcessor();

                  var methodCallsAndConstruction = method.Body.Instructions.
                      Where(i => i.OpCode == OpCodes.Newobj || i.OpCode == OpCodes.Call || i.OpCode == OpCodes.Calli || i.OpCode == OpCodes.Callvirt).
                      Select(i => (MethodReference)i.Operand);

                  foreach (var methodRef in methodCallsAndConstruction) {
                     var declaringType = methodRef.DeclaringType;
                     var declaringTypeAssembly = ((Mono.Cecil.AssemblyNameReference)declaringType.Scope).FullName;
                     var methodName = methodRef.Name;
                     System.Diagnostics.Debug.WriteLine("{0}:{1}::{2} - {3}", declaringTypeAssembly, declaringType.FullName, methodName, methodRef.ReturnType.FullName);

                     if (!ListEntry.Matches(whiteList, declaringTypeAssembly, declaringType.FullName, methodName))
                        return false;
                  }
               }
            }
         }
         return true;
      }

      // Inspects all the methods in the "Monitor" type, and see if they have our 'patch' attributes (for now: StaticMethodPatch)
      private static Dictionary<Signature, MethodDefinition> MethodPatches(TypeDefinition monitorType) {
         var patches = new Dictionary<Signature, MethodDefinition>();
         foreach (var method in monitorType.Methods) {
            var attribute = method.CustomAttributes.FirstOrDefault(attr => attr.AttributeType.FullName.Equals(typeof(MethodPatch).FullName));
            if (attribute != null) {

               IList<ParameterDefinition> parameters;
               bool isStatic = (bool)attribute.Fields.Single(arg => arg.Name == "IsStatic").Argument.Value;
               if (isStatic) {
                  // Skip last arg, the "this" (monitor) reference
                  parameters = method.Parameters.Take(method.Parameters.Count - 1).ToList();
               }
               else {
                  // Skip also the first arg (the real "this")
                  parameters = method.Parameters.Skip(1).Take(method.Parameters.Count - 2).ToList();
               }

               var signature = new Signature() {
                  ClassName = (string)attribute.Fields.Single(arg => arg.Name == "ClassName").Argument.Value,
                  MethodName = (string)attribute.Fields.Single(arg => arg.Name == "MethodName").Argument.Value,
                  IsStatic = isStatic,
                  Parameters = parameters
               };
               patches.Add(signature, method);
            }
         }

         return patches;
      }

      private static void PatchMethod(MethodDefinition method, ModuleDefinition module,
          TypeDefinition monitorType, FieldDefinition monitorRef,
          Dictionary<Signature, MethodDefinition> patches) {

         // Get a ILProcessor for the method
         var il = method.Body.GetILProcessor();

         // Retrieve instructions calling a patchable method
         var callsToPatch =
             method.Body.Instructions.
             Where(i => (i.OpCode == OpCodes.Call || i.OpCode == OpCodes.Callvirt)). // Extract method calls
             Select(i => {
                var calledMethod = (MethodReference)i.Operand;
                var key = new Signature() {
                   ClassName = calledMethod.DeclaringType.Name,
                   MethodName = calledMethod.Name,
                   IsStatic = !calledMethod.HasThis,
                   Parameters = calledMethod.Parameters
                };

                // Retrieve the patched method
                MethodDefinition newMethod = null;
                patches.TryGetValue(key, out newMethod);
                return new { calledMethod, newMethod, i };
             }). // Associate the method call with the patch
             Where(x => x.newMethod != null). // Filter calls that have actually a patch
             ToList(); // Make a list 

         foreach (var x in callsToPatch) {
            if (!x.calledMethod.HasThis) {
               System.Diagnostics.Debug.WriteLine("Patching a static method");
            }
            // Create a new instruction to call the new method
            var patchedCall = il.Create(OpCodes.Call, module.Import(x.newMethod));
            il.Replace(x.i, patchedCall);
            il.InsertBefore(patchedCall, il.Create(OpCodes.Ldsfld, monitorRef));
         }

         var objCreations =
             method.Body.Instructions.
             Where(i => i.OpCode == OpCodes.Newobj).ToList();

         var createMethod = module.Import(monitorType.Methods.Single(m => m.Name == "New"));

         // ---------
         // | Hello |
         // ---------
         // |   1   |
         // ---------
         // var foo = new Foo(1, "Hello");
         // 
         // ldc.i4.1
         // ldstr "Hello"
         // newobj .ctor
         //
         // ldc.i4.1
         // ldstr "Hello"
         // newarr
         // stloc.0      // Put arr away
         // stloc.1      // Put top of stack (Hello) away
         // ldloc.0      // arr
         // ldc.i4.1     // Index 1
         // ldloc.1      // object
         // stelem.ref   // store
         //
         // box          // Top of stack is value type? Box it
         // stloc.1      // Put top of stack (1) away
         // ldloc.0      // arr
         // ldc.i4.1     // Index 1
         // ldloc.1      // object
         // stelem.ref   // store

         foreach (var i in objCreations) {
            System.Diagnostics.Debug.WriteLine(i.Operand.ToString());

            var ctor = (MethodReference)i.Operand;
            var t = ctor.DeclaringType;

            // Skip delegate creation (do not patch them)
            if (t.Resolve().IsDelegate()) {
               continue;
            }

            var numOfArgs = ctor.Parameters.Count;

            // Create local var for array (the argument for "New")
            var objectType = module.Import(typeof(System.Object));
            var arr = new VariableDefinition(createMethod.Parameters.First().ParameterType);
            var tmp = new VariableDefinition(objectType);

            il.Body.Variables.Add(arr);
            il.Body.Variables.Add(tmp);
            il.Body.InitLocals = true;
            IList<Instruction> instructions = new List<Instruction>();

            instructions.Add(il.Create(OpCodes.Ldc_I4, numOfArgs));
            instructions.Add(il.Create(OpCodes.Newarr, objectType));
            instructions.Add(il.Create(OpCodes.Stloc, arr));

            for (int j = numOfArgs - 1; j >= 0; --j) {
               var param = ctor.Parameters[j];
               if (param.ParameterType.IsValueType) {
                  instructions.Add(il.Create(OpCodes.Box, param.ParameterType));
               }
               instructions.Add(il.Create(OpCodes.Stloc, tmp));

               instructions.Add(il.Create(OpCodes.Ldloc, arr));
               instructions.Add(il.Create(OpCodes.Ldc_I4, j));
               instructions.Add(il.Create(OpCodes.Ldloc, tmp));
               instructions.Add(il.Create(OpCodes.Stelem_Ref));
            }

            instructions.Add(il.Create(OpCodes.Ldloc, arr));
            instructions.Add(il.Create(OpCodes.Ldsfld, monitorRef));
            var patchedCall = il.Create(OpCodes.Call, createMethod.MakeGeneric(t));

            if (instructions.Count > 0) {
               var currentInsertionPoint = i;
               il.InsertBefore(i, instructions.First());
               currentInsertionPoint = instructions.First();

               foreach (var instr in instructions.Skip(1)) {
                  il.InsertAfter(currentInsertionPoint, instr);
                  currentInsertionPoint = instr;
               }
            }

            il.Replace(i, patchedCall);
         }
      }

      // A very simple demo of assembly patching
      public static byte[] PatchAssembly(byte[] compiledSnippet, string className) {

         var monitorModule = ModuleDefinition.ReadModule(typeof(Monitor).Assembly.Location);
         var monitorType = monitorModule.GetType("Pumpkin.Monitor");

         using (var memoryStream = new MemoryStream(compiledSnippet)) {
            var module = ModuleDefinition.ReadModule(memoryStream);

            // Retrieve the target class (the snippet class) we want to patch
            var targetType = module.Types.Single(t => t.FullName == className);

            var monitorInstance = new FieldDefinition(MonitorFieldName, Mono.Cecil.FieldAttributes.Static | Mono.Cecil.FieldAttributes.Public, module.Import(monitorType));
            targetType.Fields.Add(monitorInstance);

            // Load the list of patchabel methods
            var patches = MethodPatches(monitorType);

            // Patch all of them!
            foreach (var method in targetType.Methods) {
               PatchMethod(method, module, monitorType, monitorInstance, patches);
            }

            // Write the module
            using (var outputAssembly = new MemoryStream()) {
               module.Write(outputAssembly);
               return outputAssembly.ToArray();
            }
         }
      }
   }
}
