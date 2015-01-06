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
      public int isAborting;
   }

   public class DomainPool {

      const int MaxReuse = 100; // How many times we want to reuse an AppDomain? (0 = infinite)
      const int MaxZombies = 20; // How many "zombie" (potentially undead/leaking domains) we tolerate? (0 = infinite)
      const int NumberOfDomainsInPool = 1;
      const int runningThreshold = 3 * 1000; // In milliseconds

      // Using separate arrays may improve efficiency, especially if 
      // we want to go (later) for a lock-free approach.
      object poolLock = new object();
      PooledDomainData[] poolDomains = new PooledDomainData[NumberOfDomainsInPool];

      static string threadsExaustedAbortToken = "AbortTooManyThreads";
      static string timeoutAbortToken = "AbortTimeout";

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
            var thread = CreateDomainThread(i);
            thread.Start();
         }
      }


      private PooledDomainData FindByAppDomainId(int appDomainId) {
         lock (poolLock) {
            for (int i = 0; i < NumberOfDomainsInPool; ++i) {
               if (poolDomains[i] != null && poolDomains[i].domainId == appDomainId)
                  return poolDomains[i];
            }
            return null;
         }
      }

      private void WatchdogThreadFunc() {

         while (!isExiting) {

            // This is a watchdog; it does not need to be precise.
            // It can be improved by recomputing the next check time (i.e. when a snippet expires)
            // at each submission, but for now we are good.

            // We sleep for a while, than check 
            try {
               //Thread.Sleep(1000);
               HostEvent hostEvent;
               if (defaultDomainManager.GetHostMessage(1000, out hostEvent)) {
                  System.Diagnostics.Debug.WriteLine("Watchdog: message from host for AppDomain " + hostEvent.appDomainId);
                  // Process event
                  switch ((HostEventType)hostEvent.eventType) {
                     case HostEventType.OutOfTasks: {
                           PooledDomainData poolDomain = FindByAppDomainId(hostEvent.appDomainId);
                           if (poolDomain != null) {
                              if (Interlocked.CompareExchange(ref poolDomain.isAborting, 1, 0) == 0) {
                                 poolDomain.mainThread.Abort(threadsExaustedAbortToken);
                              }
                           }
                        }
                        break;
                  }
               }
               else {
                  long currentTime = GetTimestamp();
                  System.Diagnostics.Debug.WriteLine("Watchdog: check at " + currentTime);

                  for (int i = 0; i < NumberOfDomainsInPool; ++i) {
                     // Something is running?
                     PooledDomainData poolDomain = null;
                     lock (poolLock) {
                        poolDomain = poolDomains[i];
                     }

                     if (poolDomain != null) {
                        long snippetStartTime = Thread.VolatileRead(ref poolDomain.timeOfSubmission);
                        if (snippetStartTime > 0) {
                           // For too long?
                           long millisRunning = currentTime - snippetStartTime;
                           if (millisRunning > runningThreshold) {
                              // Abort Thread i / Unload domain i
                              // Calling abort here is fine: the host will escalate it for us if it times
                              // out. Otherwise, we can still catch it and reuse the AppDomain
                              System.Diagnostics.Debug.WriteLine("Timeout: aborting thread #{0} in domain {1} ({2} ms)", i, poolDomains[i].domainId, millisRunning);

                              // Avoid to call Abort twice
                              Thread.VolatileWrite(ref poolDomains[i].timeOfSubmission, 0);
                              poolDomains[i].mainThread.Abort(timeoutAbortToken);
                           }
                           // Check also for appDomain.MonitoringTotalProcessorTime
                           //else if (poolDomains[i].appDomain.MonitoringTotalProcessorTime > MaxCpuTime)
                           //   //TODO
                           //}
                        }

                        // If our thread was aborted rudely, we need to create a new one
                        if (poolDomain.mainThread == null || poolDomain.mainThread.ThreadState == System.Threading.ThreadState.Aborted) {
                           defaultDomainManager.HostUnloadDomain(poolDomain.domainId);
                           poolDomains[i] = new PooledDomainData();
                           var thread = CreateDomainThread(i);
                           thread.Start();
                        }
                     }
                  }
               }

               // Ensure there is at least a thread in the pool
               if (numberOfThreadsInPool < 1) {
                  poolDomains[0] = new PooledDomainData();
                  var thread = CreateDomainThread(0);
                  thread.Start();
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
         if (MaxZombies > 0 && defaultDomainManager.GetNumberOfZombies() > MaxZombies) {
            // TODO: break connection, signal our "partner"/ monitor we are going down
            System.Diagnostics.Debug.WriteLine("RecycleDomain - escalate to process recycling");
            this.Exit();
         }
         else {
            // We are saying "goodbye". Decrement the number of threads in the pool                        
            Interlocked.Decrement(ref numberOfThreadsInPool);
            int domainId = 0;
            lock (poolLock) {
               domainId = poolDomains[index].domainId;
               poolDomains[index] = null;
            }
            try {
               defaultDomainManager.HostUnloadDomain(domainId);
               System.Diagnostics.Debug.WriteLine("RecycleDomain - Domain " + domainId + " unload");
            }
            catch (CannotUnloadAppDomainException ex) {
               System.Diagnostics.Debug.WriteLine("RecycleDomain - CannotUnloadAppDomainException: ");
               if (ex.InnerException != null)
                  System.Diagnostics.Debug.WriteLine(ex.InnerException.GetType().Name + " - " + ex.InnerException.Message);
               System.Diagnostics.Debug.WriteLine(ex.ToString());
            }
         }
      }

      private static long GetTimestamp() {
         long frequency = Stopwatch.Frequency;
         return Stopwatch.GetTimestamp() / (frequency / 1000);
      }

      private Thread CreateDomainThread(int threadIndex) {
         System.Diagnostics.Debug.WriteLine("CreateDomainThread: " + threadIndex);

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
                  defaultDomainManager.ResetContextFor(myPoolDomain);

                  // And here is were we "rent" one AppDomain and use it to run a snippet
                  SnippetInfo snippetToRun = null;
                  try {
                     snippetToRun = snippetsQueue.Take();
                  }
                  catch (InvalidOperationException) {
                     // Someone called "complete". No more snippets in this process.
                     // We want to exit the pool.
                     return;
                  }
                  
                  if (snippetToRun != null) {

                     System.Diagnostics.Debug.WriteLine("Starting snippet '" + snippetToRun.methodName + "' in domain " + myPoolDomain.domainId);
                     
                     SnippetResult result = new SnippetResult();
                     bool recycleDomain = false;

                     try {
                        Interlocked.Increment(ref myPoolDomain.numberOfUsages);
                        // Record when we started
                        long startTimestamp = GetTimestamp();
                        System.Diagnostics.Debug.WriteLine("Starting execution at " + startTimestamp);
                        Thread.VolatileWrite(ref myPoolDomain.timeOfSubmission, startTimestamp);

                        // Thread transitions into the AppDomain
                        // This function DOES NOT throw
                        result = manager.InternalRun(appDomain, snippetToRun.assemblyFile, snippetToRun.mainTypeName, snippetToRun.methodName, true);


                        // ...back to the main AppDomain
                        Debug.Assert(AppDomain.CurrentDomain.IsDefaultAppDomain());
                        // Flag it as "not executing"
                        Thread.VolatileWrite(ref myPoolDomain.timeOfSubmission, 0);                        
                     }
                     catch (ThreadAbortException ex) {
                        // Someone called abort on us. 
                        result.exception = ex;

                        // It may be possible to use this domain again, otherwise we will recycle
                        if (Object.Equals(ex.ExceptionState, timeoutAbortToken)) {
                           // The abort was issued by us because we timed out
                           System.Diagnostics.Debug.WriteLine("Thread Abort due to timeout");
                           result.status = SnippetStatus.Timeout;                           
                        }
                        else if (Object.Equals(ex.ExceptionState, threadsExaustedAbortToken)) {
                           System.Diagnostics.Debug.WriteLine("Thread Abort due to thread exaustion");
                           result.status = SnippetStatus.ResourceError;
                        }
                        else {
                           // If it wasn't us, give us time to record the result; we will recycle (unload) the domain
                           System.Diagnostics.Debug.WriteLine("Thread Abort due to external factors");
                           result.status = SnippetStatus.CriticalError;
                           recycleDomain = true;
                        }

                        // Give us time to record the result; if needed, we will recycle (unload) the domain later
                        Thread.ResetAbort();
                     }
                     catch (Exception ex) {
                        result.exception = ex;
                        // Check if someone is misbehaving, throwing something else to "mask" the TAE
                        if (Thread.CurrentThread.ThreadState == System.Threading.ThreadState.AbortRequested) {
                           result.status = SnippetStatus.CriticalError;
                           recycleDomain = true;

                           // Give us time to record the result; we will recycle (unload) the domain
                           Thread.ResetAbort();
                        }
                        else {
                           result.status = SnippetStatus.ExecutionError;
                        }
                     }
                     // "Although C# only allows you to throw objects of type Exception and types deriving from it, 
                     // other languages don’t have any such restriction."
                     // http://weblogs.asp.net/kennykerr/introduction-to-msil-part-5-exception-handling
                     // It turns out this is no longer necessary
                     // http://blogs.msdn.com/b/jmanning/archive/2005/09/16/469091.aspx
                     // catch { }

                     // No need to catch StackOverflowException; the Host will escalate to a (rude) domain unload
                     // for us
                     // TODO: check that AppDomain.DomainUnload is called anyway!

                     result.executionTime = GetTimestamp() - Thread.VolatileRead(ref myPoolDomain.timeOfSubmission);

                     // Before looping, check if we are OK; we reuse the domain only if we are not leaking
                     int threadsInDomain = defaultDomainManager.GetThreadCount(appDomain.Id);
                     int memoryUsage = defaultDomainManager.GetMemoryUsage(appDomain.Id);

                     System.Diagnostics.Debug.WriteLine("============= AppDomain {0} =============", appDomain.Id);
                     System.Diagnostics.Debug.WriteLine("Finished in: {0}", result.executionTime);
                     System.Diagnostics.Debug.WriteLine("Status: {0}", result.status);
                     if (result.exception != null)
                        System.Diagnostics.Debug.WriteLine("Exception: " + result.exception.Message);
                     System.Diagnostics.Debug.WriteLine("Threads: {0}", threadsInDomain);
                     System.Diagnostics.Debug.WriteLine("Memory: {0}", memoryUsage);
                     System.Diagnostics.Debug.WriteLine("Status: {0}", memoryUsage);
                     System.Diagnostics.Debug.WriteLine("========================================");

                     if (threadsInDomain > 1) {
                        // The snippet is leaking threads

                        // Flag snippet as "leaking" 
                        result.status = SnippetStatus.CriticalError;
                        System.Diagnostics.Debug.WriteLine("Leaking snippet");

                        recycleDomain = true;
                        
                     }                     
                     else if (MaxReuse > 0 && myPoolDomain.numberOfUsages >= MaxReuse) {
                        System.Diagnostics.Debug.WriteLine("Domain too old");

                        // Same if the domain is too old
                        recycleDomain = true;
                     } 
                   
                     // TODO: return the result to the caller

                     if (recycleDomain) {
                        System.Diagnostics.Debug.WriteLine("Recycling domain...");
                        RecycleDomain(threadIndex);
                     }
                     else {
                        // Otherwise, ensure that what was allocated by the snippet is freed
                        GC.Collect();
                        System.Diagnostics.Debug.WriteLine("MonitoringSurvivedMemorySize {0}", appDomain.MonitoringSurvivedMemorySize);
                        System.Diagnostics.Debug.WriteLine("MonitoringTotalAllocatedMemorySize {0}", appDomain.MonitoringTotalAllocatedMemorySize);
                        System.Diagnostics.Debug.WriteLine("MonitoringTotalProcessorTime {0}", appDomain.MonitoringTotalProcessorTime);
                     }
                  }
               }
            }
            catch (Exception ex) {
               System.Diagnostics.Debug.WriteLine("Exception caught:\n" + ex.ToString());
               RecycleDomain(threadIndex);
            }
         });

         thread.Name = "DomainPool thread " + threadIndex;
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
               var thread = CreateDomainThread(emptySlotIdx);
               thread.Start();
            }
         }

         snippetsQueue.Add(snippetInfo);
      }
   }
}
