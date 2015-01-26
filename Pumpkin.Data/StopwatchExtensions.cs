using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Pumpkin.Data {
   public static class StopwatchExtensions {      

      public static long GetTimestampMillis() {
         long frequency = Stopwatch.Frequency;
         return Stopwatch.GetTimestamp() / (frequency / 1000);
      }
   }
}
