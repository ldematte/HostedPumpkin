using System;
using System.Threading.Tasks;

namespace Pumpkin.Data {
   public static class TaskExtensions {
      // Supress warning CS4014 without using the "evil" async void
      public static void FireAndForget(this Task t) { }
   }
}
