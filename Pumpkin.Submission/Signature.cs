using Mono.Cecil;
using System;
using System.Collections.Generic;
using System.Linq;


namespace Pumpkin {
    public class Signature {

        public string ClassName { get; set; }
        public string MethodName { get; set; }

        public bool IsStatic { get; set; }

        public IList<ParameterDefinition> Parameters { get; set; }

        public override bool Equals(object obj) {
            var other = obj as Signature;
            if (other == null)
                return false;

            return other.ClassName.Equals(ClassName) &&
                other.MethodName.Equals(MethodName) &&
                other.IsStatic == IsStatic &&
                other.Parameters.Count == Parameters.Count &&
                other.Parameters.
                    Zip(Parameters, (p1, p2) => p1.ParameterType.FullName.Equals(p2.ParameterType.FullName)).
                    All(p => p);
        }

        public override int GetHashCode() {
            // TODO: review
            return ClassName.GetHashCode() ^
                   MethodName.GetHashCode() ^
                   Parameters.Count;
        }
    }
}