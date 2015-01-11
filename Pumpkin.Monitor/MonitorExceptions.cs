using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.Serialization;
using System.Security;
using System.Text;

namespace Pumpkin {

    [Serializable]
    [SecuritySafeCritical]
    public class TooManyAllocationsException : Exception {

        public TooManyAllocationsException() { }
        public TooManyAllocationsException(SerializationInfo info, StreamingContext context)
            : base (info, context) { }
    }

    [Serializable]
    [SecuritySafeCritical]
    public class TooManyThreadsException : Exception {

        public TooManyThreadsException() { }
        public TooManyThreadsException(SerializationInfo info, StreamingContext context)
            : base(info, context) { }
    }
}
