using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Pumpkin {
    public class ListEntry {

        private readonly string assemblyName;
        private readonly string typeName;
        private readonly string methodName;

        // Allow/disallow the use of an assembly altogether
        public ListEntry(string assemblyName) {
            this.assemblyName = assemblyName;
        }

        // Allow/disallow the use of a type altogether
        public ListEntry(string assemblyName, string typeName) {
            this.assemblyName = assemblyName;
            this.typeName = typeName;
        }

        // Allow/disallow the use of a method
        public ListEntry(string assemblyName, string typeName, string methodName) {
            this.assemblyName = assemblyName;
            this.typeName = typeName;
            this.methodName = methodName;
        }

        // TODO: may use regexp, if we like it!
        private static bool WildcardOrEquals(string thisName, string otherName) {
            return (String.IsNullOrEmpty(thisName) || thisName.Equals(otherName));
        }

        public static bool Matches(List<ListEntry> list, string assemblyName, string typeName, string methodName) {
            var matchingAssemblies = list.Where(e => WildcardOrEquals(e.assemblyName, assemblyName));
            if (matchingAssemblies.Any()) {
                var matchingTypes = matchingAssemblies.Where(e => WildcardOrEquals(e.typeName, typeName));
                if (matchingTypes.Any()) {
                    var matchingMethods = matchingTypes.Where(e => WildcardOrEquals(e.methodName, methodName));
                    return matchingMethods.Any();
                }
            }
            return false;
        }

    }
}
