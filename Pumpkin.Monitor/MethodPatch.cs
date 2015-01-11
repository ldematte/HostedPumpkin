using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Pumpkin {

    [AttributeUsage(AttributeTargets.Method)]
    public class MethodPatch: Attribute {
        public string ClassName;
        public string MethodName;
        public bool IsStatic = false;
    }
}
