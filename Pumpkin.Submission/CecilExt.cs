

using Mono.Cecil;
using System;
namespace Pumpkin {

    public static class CecilExt {

        public static MethodReference MakeGeneric(this MethodReference method, params TypeReference[] args) {
            if (args.Length == 0)
                return method;

            if (method.GenericParameters.Count != args.Length)
                throw new ArgumentException("Invalid number of generic type arguments supplied");

            var genericTypeRef = new GenericInstanceMethod(method);
            foreach (var arg in args)
                genericTypeRef.GenericArguments.Add(arg);

            return genericTypeRef;
        }

        public static bool IsDelegate(this TypeDefinition typeDefinition) {
            if (typeDefinition.BaseType == null) {
                return false;
            }
            return typeDefinition.BaseType.FullName == "System.MulticastDelegate";
        }
    }
}