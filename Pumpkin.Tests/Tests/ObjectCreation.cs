using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Snippets {
    public class ObjectCreation {

        public class Foo {
            private int bar;
            private string baz;

            public Foo(int bar, string baz) {
                this.bar = bar;
                this.baz = baz;
            }

            public string Get() {
                return String.Format("{0} - {1}", bar, baz);
            }
        }

        public static void SnippetMain() {

            for (int i = 0; i < 10; ++i) {
                var foo = new Foo(i, "Hello");
                Console.WriteLine(foo.Get());
            }

        }
    }
}