using System;
using System.Threading;

namespace Snippets {

    public static class ForkBomb {

#region snippet

        public static void ThreadFunc() {
            while(true) {
                new Thread(ThreadFunc).Start();
            }
        }

        public static void SnippetMain() {
            ThreadFunc();
        }

#endregion

    }

}
