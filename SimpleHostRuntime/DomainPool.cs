using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace SimpleHostRuntime {

   public class SnippetInfo {
      // TODO: prop
      public byte[] assemblyFile;
      public string mainTypeName;
      public string methodName;
   }

   public class PooledDomainData {
      // TODO: prop
      public long timeOfSubmission;
      public int domainId;
      public Thread mainThread;
      public int numberOfUsages;
   }

   public class DomainPool {

      const int MaxReuse = 0; // How many times we want to reuse an AppDomain? (0 = infinite)
      const int MaxZombies = 20; // How many "zombie" (potentially undead/leaking domains) we tolerate? (0 = infinite)
      const int NumberOfDomainsInPool = 16;
      const int runningThreshold = 3 * 1000; // 3 seconds

      // Using separate arrays may improve efficiency, especially if 
      // we want to go (later) for a lock-free approach.
      object poolLock = new object();
      PooledDomainData[] poolDomains = new PooledDomainData[NumberOfDomainsInPool];

      static object timeoutAbortToken = new object();

      Thread watchdogThread;

      BlockingCollection<SnippetInfo> snippetsQueue = new BlockingCollection<SnippetInfo>();

      bool isExiting = false;

      private readonly SimpleHostAppDomainManager defaultDomainManager;

      public DomainPool(SimpleHostAppDomainManager defaultDomainManager) {
         this.defaultDomainManager = defaultDomainManager;
         defaultDomainManager.DomainUnload += domainManager_DomainUnload;
                  
         InitializeDomainPool();
      }

      void domainManager_DomainUnload(int domainId) {
         lock (poolLock) {
            for (int i = 0; i < NumberOfDomainsInPool; ++i) {
               if (poolDomains[i].domainId == domainId) {
                  poolDomains[i] = null;
                  return;
               }
            }
         }
      }
      

      private void InitializeDomainPool() {

         watchdogThread = new Thread(WatchdogThreadFunc);
         watchdogThread.Start();

         for (int i = 0; i < NumberOfDomainsInPool; ++i) {
            poolDomains[i] = new PooledDomainData();
            CreateDomainThread(i);            
         }
      }

      private void WatchdogThreadFunc() {

         while (!isExiting) {

            // This is a watchdog; it does not need to be precise.
            // It can be improved by recomputing the next check time (i.e. when a snippet expires)
            // at each submission, but for now we are good.

            // We sleep for a while, than check 
            try {
               Thread.Sleep(1000);

               long currentTime = Stopwatch.GetTimestamp();

               for (int i = 0; i < NumberOfDomainsInPool; ++i) {
                  // Something is running?
                  if (poolDomains[i] != null && poolDomains[i].timeOfSubmission > 0) {
                     // For too long?
                     if (currentTime - poolDomains[i].timeOfSubmission > runningThreshold) {
                        // Abort Thread i / Unload domain i
                        // Calling abort here is fine: the host will escalate it for us if it times
                        // out. Otherwise, we can still catch it and reuse the AppDomain
                        poolDomains[i].mainThread.Abort(timeoutAbortToken);
                    
                        // TODO: check also for appDomain.MonitoringTotalProcessorTime?
                     }
                  }
               }
            }
            catch (ThreadInterruptedException) {
               // Do we need to exit? Simply cycle back to check exit status
            }
         }
      }

      public void Exit() {
         // CompleteAdding will make threads in our pool to exit
         isExiting = true;
         snippetsQueue.CompleteAdding();
         watchdogThread.Interrupt();
         watchdogThread.Join();
      }

      private long numberOfThreadsInPool = 0;

      private void RecycleDomain(int index) {
         lock (poolLock) {
            poolDomains[index] = null;
         }

         if (MaxZombies > 0 && defaultDomainManager.GetNumberOfZombies() > MaxZombies) {
            // TODO: break connection, signal our "partner"/ monitor we are going down
            this.Exit();
         }
         else {
            AppDomain.Unload(AppDomain.CurrentDomain);
            Interlocked.Decrement(ref numberOfThreadsInPool);
         }
      }

      private Thread CreateDomainThread(int threadIndex) {

         var myPoolDomain = poolDomains[threadIndex];
                 
         var thread = new Thread(() => {
            Interlocked.Increment(ref numberOfThreadsInPool);
            // Here we enforce the "one domain, one thread" relationship
            try {
               var appDomain = AppDomainHelpers.CreateSandbox("Host Sandbox");
               var manager = (SimpleHostAppDomainManager)appDomain.DomainManager;

               lock (poolLock) {                  
                  myPoolDomain.domainId = appDomain.Id;
               }

               while (!snippetsQueue.IsCompleted) {
                  defaultDomainManager.ResetContextFor(myPoolDomain.domainId);

                  // And here is were we "rent" one AppDomain and use it to run a snippet
                  SnippetInfo snippetToRun = null;
                  try {
                     snippetToRun = snippetsQueue.Take();
                  }
                  catch (InvalidOperationException) {
                     // Someone called "complete".
                     // We want to exit the pool
                     return;
                  }

                  SnippetResult result = new SnippetResult();

                  if (snippetToRun != null) {

                     Stopwatch stopwatch = Stopwatch.StartNew();

                     try {
                        Interlocked.Increment(ref myPoolDomain.numberOfUsages);
                        // Record when we started
                        myPoolDomain.timeOfSubmission = Stopwatch.GetTimestamp();

                        // Thread transitions into the AppDomain
                        // This function DOES NOT throw
                        manager.InternalRun(appDomain, snippetToRun.assemblyFile, snippetToRun.mainTypeName, snippetToRun.methodName, true);


                        // ...back to the main AppDomain
                        Debug.Assert(AppDomain.CurrentDomain.IsDefaultAppDomain());
                        // Flag it as "not executing"
                        myPoolDomain.timeOfSubmission = 0;                       

                        // React to our DomainManager status (IF we are still alive here, there was no thread abort or domain unload!)
                        result.status = SnippetStatus.Success;
                     }
                     catch (ThreadAbortException ex) {
                        // Someone called abort on us. 
                        // It may be possible to use this domain again, otherwise we will recycle
                        if (ex.ExceptionState == timeoutAbortToken) {
                           // The abort was issued by us because we timed out
                           // Set exit status
                           result.status = SnippetStatus.Timeout;
                           result.exception = ex;
                           Thread.ResetAbort(); // Reset and try to reuse this thread/AppDomain
                        }
                        // If it wasn't us, let the abort continue. Our policy will take down the domain
                     }
                     catch (Exception ex) {
                        result.exception = ex;
                        // Check if someone is misbehaving, throwing something else to "mask" the TAE
                        if (Thread.CurrentThread.ThreadState == System.Threading.ThreadState.AbortRequested) {
                           result.status = SnippetStatus.CriticalError;
                           Thread.ResetAbort();
                        }
                        else {
                           result.status = SnippetStatus.ExecutionError;
                        }
                     }
                     // "Although C# only allows you to throw objects of type Exception and types deriving from it, 
                     // other languages don’t have any such restriction."
                     // http://weblogs.asp.net/kennykerr/introduction-to-msil-part-5-exception-handling
                     // No longer necessary
                     // http://blogs.msdn.com/b/jmanning/archive/2005/09/16/469091.aspx
                     // catch { }

                     // No need to catch StackOverflowException; the Host will escalate to a (rude) domain unload
                     // for us
                     // TODO: check that AppDomain.DomainUnload is called anyway!

                     result.executionTime = stopwatch.ElapsedMilliseconds;

                     // Before looping, check if we are OK; we reuse the domain only if we are not leaking
                     int threadsInDomain = defaultDomainManager.GetThreadCount(appDomain.Id);
                     int memoryUsage = defaultDomainManager.GetMemoryUsage(appDomain.Id);

                     System.Diagnostics.Debug.WriteLine("============= AppDomain %d =============", appDomain.Id);
                     System.Diagnostics.Debug.WriteLine("Finished in: %d", result.executionTime);
                     System.Diagnostics.Debug.WriteLine("Status: %d", result.status);
                     if (result.exception != null)
                        System.Diagnostics.Debug.WriteLine("Exception: %s", result.exception.Message);
                     System.Diagnostics.Debug.WriteLine("Threads: %d", threadsInDomain);
                     System.Diagnostics.Debug.WriteLine("Memory: %d", memoryUsage);
                     System.Diagnostics.Debug.WriteLine("Status: %d", memoryUsage);
                     System.Diagnostics.Debug.WriteLine("========================================");

                     if (threadsInDomain > 1) {
                        // The snippet is leaking threads

                        // Flag snippet as "leaking" 
                        result.status = SnippetStatus.CriticalError;
                        System.Diagnostics.Debug.WriteLine("Leaking snippet");

                        // We are saying "goodbye". Decrement the number of threads in the pool
                        RecycleDomain(threadIndex);                   
                     }
                     // Same if the domain is too old
                     else if (MaxReuse > 0 && myPoolDomain.numberOfUsages >= MaxReuse) {
                        System.Diagnostics.Debug.WriteLine("Domain too old");
                        RecycleDomain(threadIndex);
                     }                     
                     else {
                        // Otherwise, ensure that what was allocated by the snippet is freed
                        GC.Collect();
                        System.Diagnostics.Debug.WriteLine("MonitoringSurvivedMemorySize %d: ", appDomain.MonitoringSurvivedMemorySize);
                        System.Diagnostics.Debug.WriteLine("MonitoringTotalAllocatedMemorySize %d: ", appDomain.MonitoringTotalAllocatedMemorySize);
                        System.Diagnostics.Debug.WriteLine("MonitoringTotalProcessorTime %d: ", appDomain.MonitoringTotalProcessorTime);
                     }
                  }
               }
            }
            catch (Exception ex) {
               System.Diagnostics.Debug.WriteLine("Exception caught:\n{0}", ex.ToString());
               RecycleDomain(threadIndex);
            }
         });

         myPoolDomain.mainThread = thread;
         return thread;
      }

      internal void SubmitSnippet(SnippetInfo snippetInfo) {
         // check if we need to re-introduce some domains in the pool
         if (Interlocked.Read(ref numberOfThreadsInPool) < NumberOfDomainsInPool) {
            // A sort of "double-check optimization": we enter here only if there is a shortage of threads/domains, but
            // we actually create one only if there is a slot
            int emptySlotIdx = -1;
            lock (poolLock) {
               // find the empty slot
               for (int i = 0; i < NumberOfDomainsInPool; ++i) {
                  if (poolDomains[i] == null) {
                     poolDomains[i] = new PooledDomainData();
                     emptySlotIdx = i;
                  }
               }
            }

            if (emptySlotIdx >= 0) {
               CreateDomainThread(emptySlotIdx);
            }
         }

         snippetsQueue.Add(snippetInfo);
      }
   }
}
