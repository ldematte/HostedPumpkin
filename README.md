HostedPumpkin
=============

Proof-Of-Concept (POC) for the submission, compilation and execution of C# code snippets.

The solution is composed by the following sub-project:

- The main project is a unmanaged C++ project, which hosts the CLR using the Hosting API. The Host uses two helper assemblies: 
 - a custom [AppDomainManager](https://msdn.microsoft.com/en-us/library/system.appdomainmanager(v=vs.110).aspx) (project _SimpleHostRuntime_)
 - a set of common classes (project _Pumpkin.Monitor_) loaded with the snippet assembly
- The _Pumpkin.Submission_ project compiles snippets using the [CodeDOM compiler](https://msdn.microsoft.com/en-us/library/system.codedom.compiler(v=vs.110).aspx) (i.e. going through `csc.exe`), then serializes the assemblies.
- The _Pumpkin.Data_ project provides supporting functions, for example it helps in storing and retrieving compiled snippets in/from a DB
- The _Pumpkin.Web_ project is a very simple ASP.NET MVC project, with pages for submitting a new snippet, listing all the snippets and execute them.

![](https://github.com/ldematte/SimpleHost/blob/master/resources/Components.png)

The goal of this POC is to take some C#, compile it, run it. Easy :) 
More precisely, we want to execute third-party code (a “snippet”) in a safe, reliable and efficient way.

#### The problems ####

We want constraints on what the code can do.

We want safety/security (for example: you do not want snippets to use WMI to shutdown the machine, or open a random port, install a torrent server, read configuration files from the server, erase files...), 

We want to be able to handle dependencies in a sensible way (some assemblies/classes/methods just do no make any sense: Windows Forms? Workflow Foundations? Sql?) 

We want to monitor and cap resource usage:

- no snippets that do not terminate -no, we don't want to save the Halting Problem, but we want to timeout and bring down the offending snippet as cleanly as possible, reclaiming the allocated resources (memory and threads)-, 
- no snippets that try to clog the system by spawning too many threads, 
- no snippets that take all the available memory, and so on).

#### Which "security" do we want to provide? ####

1. no "unsafe"
2. no p/invoke or unmanaged code
3. nothing from the server that runs the snippet is accessible: no file read, no access to local registry (read OR write!)
4. no network permission
5. whitelist of permissions and assemblies

#### Which "resources" we want to control? ####
1. limit execution time
 - running time/execution time
2. limit thread creation (avoid "fork-bombs")
 - deny (or handle in a sensible way) access to named kernel objects (e.g. named semaphores.. you do not want some casual interaction with them!)
3. limit process creation (zero - no spawning of processes. Can fall back to the "security" category: deny process creation)
4. limit memory usage
5. limit file usage (no files. Can fall back to the "security" category)
6. limit network usage (no network. Can fall back to the "security" category)
 - in the future: virtual network, virtual files?
7. limit output (Console.WriteLine, Debug.out...)
 - and of course redirect it

#### Performance considerations ####
1. Snippet execution should be fast (< 1sec. for simple snippets, i.e. overhead << 1sec.)
2. Scaling up: can execute hundreds of snippets concurrently (on a single/few machines)

Background
----------

In order to obtain what we want, it is possible to act at two levels: control at compilation time and control at execution time.

w.r.t control at execution time, it is possible to act in three ways:

- AppDomain sandboxing: "classical" way, tested, good for security
- Hosting the CLR: greater control on resource allocation
- Using the debugging API/profiling API: even greater control on the executed program. Can be slower, can have side effects.

### Control at compilation time

One possibility is to pre-process the source code, at compilation time, to recognize, remove, and inject control "probes" inside the code.
These "probes" can then be used to monitor resource usage (i.e. allocation, CPU usage, thread creation, ...). 
Classes that we deem unsafe can either be forbidden (raising an error and rejecting the snippet), or replaced.

We considered [Roslyn](http://roslyn.codeplex.com/) the new (and open-source) C# compiler, but discarded it, as one requirement is to let the user choose different, older, C# versions.
We the used the `CodeDOM` compiler, which is just a managed wrapper around the actual `csc.exe` (of which there are multiple instances on a machine, one for each SDK version).

### Control at execution time

#### AppDomain sandboxing 

Using the protection given by AppDomains is the bare minimum: we want to run each snippet in its own [separate AppDomain sandbox](https://msdn.microsoft.com/en-us/library/bb763046(v=vs.110).aspx), with a custom `PermissionSet`, starting with an (almost) empty one. 
AppDomain sandboxing helps with the security aspect, but not for the resource control, nor does it give enough protection in case of faults (for example, thread aborts, unhandled exceptions, failed AppDomain unloads, ...)

#### Hosting the CLR

Building a custom CLR host means running the CLR (and the snippets) inside our own executable, which is notified of several events and acts as a proxy between the managed code and several unmanaged resources, like tasks (threads) and memory.

Microsoft provides an [Hosting API](http://msdn.microsoft.com/en-us/library/dd380850(v=vs.110).aspx), which is used for very similar purposes (protection and enforcement) by ASP.NET and SQL Server
For example, you can control and replace all the native implementations of ["task-related" functions](http://msdn.microsoft.com/en-us/library/ms164562(v=vs.110).aspx)

#### Debugging and CLR Profiling API

These API (the [Managed Debugging API](https://msdn.microsoft.com/en-us/library/vstudio/bb384605(v=vs.90).aspx), [ClrMD](http://blogs.msdn.com/b/dotnet/archive/2013/05/01/net-crash-dump-and-live-process-inspection.aspx?) and CLR Profiling API (see [1](http://msdn.microsoft.com/en-us/library/bb384493.aspx) and  [2](http://msdn.microsoft.com/en-us/library/dd380851(v=vs.110).aspx)) give you plenty of control: you can "see" and "do" everything that a debugger sees or does.

This includes step-by-step execution, thread creation, exit, on-the-fly IL rewriting (you are notified when a method is JIT-ed and you can modify the IL stream before JIT), effectively gaining complete control on the debuggee.


The POC
-------

We divided the process in two: _Snippet submission_ and *snippet execution*.
At submission time, a snippet is compiled using `csc.exe` (through the `CodeDOM` classes).

### Snippet submission

A first attempt was made using only managed resources; this approach was based on isolation through AppDomains and supervising through some "probes": methods which could have been problematic, for example all constructors, all thread functions/delegates, etc. where patched (at IL level) so that they added some "fail-safes" (`try`-`catch` to avoid unhandled exceptions, for example) and code to monitor resource usage.

The _snippet submission_ part was considerably prominent: execution was protected by either .NET security (AppDomain sandbox) or our own injected code (for the resource usage part).

Since compilation with `CodeDOM` is a black box, and we could not use Roslyn due to other requirements, we used [Mono.Cecil](https://github.com/jbevain/cecil) to inspect and patch the IL code.

We then went for a different approach (adding Hosting into the mix), but the submission part still uses Cecil to inspect and rewrite code (substituting type and functions with our own; for example, we use it to "redirect" calls to `Console.WriteLine` to a memory buffer).

###Snippet execution

For the snippet execution part, we use a combination of AppDomains for security and isolation, and CLR Hosting to track and control execution.
AppDomains are used, to the degree in which this is possible, to isolate the snippet: two snippets cannot interact in any way, and a misbehaving snippet should not affect others. 

They are also a nice compromise in terms of balancing performance/scalability and safety/control/isolation.

Reusing and sharing processes and threads has a big advantage in terms of startup speed, resource usage and scalability. The price is that you lose the nice OS-provided isolation  and OS-provided cleanup.

We implemented an AppDomain pool, in which each AppDomain follows a series of policies to decide if it can be reused for another snippet execution, recycled (unloaded and replaced by a new one) and to track resource usage and faults.

Resource tracking is done by the Host interfaces, with the help of a `HostContext` class; faults are detected using a combination of managed code inside the the `AppDomainManager` and in the Host, by using escalation policies (more later)

![](https://github.com/ldematte/SimpleHost/blob/master/resources/ThreadFlow.png)


##### Host interfaces, HostContext 

We implemented all the available Host Manager interfaces (see [1](https://msdn.microsoft.com/en-us/library/ms404385(v=vs.110).aspx) and [2](https://msdn.microsoft.com/en-us/magazine/cc163567.aspx)). If the host implements a manager, the CLR does not create the associated resource (thread, memory, etc.) by itself, but offloads its management to the Host. So we intercepted virtually all calls that the code could make to the CLR and "re-implement" them with unmanaged primitives. 

In short, the CLR inside is full of places where it either ask the host to provide some functionality, or do it directly if no host is provided. An example from (managed) thread creation:

    IHostTaskManager *hostTaskManager = CorHost2::GetHostTaskManager();
    if (hostTaskManager) {
       hostTaskManager->GetCurrentTask(&curHostTask);
    }

The interfaces provided by the Hosting API are implemented in the more straightforward way possible, with some exceptions. The goal is to mimic really closely what the "standard" (non-hosted) version of the CLR does. Obviously, we add code to record what is going on: the goal is to collect resource usage, per snippet, in order to cap them, or  prevent some events from happening (setting thread priority above normal? No no) and react to problems (a snippet is creating thousands of threads? Stop it and notify the `AppDomainManager`: it will try to Abort it and then (eventually) escalate the issue).

All the classes implementing the hosting interfaces and managers receive a `HostContext`, which records information about resources in order to track them. It tracks AppDomain - Thread relationships(*), Managed threads (called `CLRTask`s in the Hosting API) - Native thread relationships (called `HostTask`s in the Hosting API).

`HostContext` is a coclass, so it can be accessed from managed code(**). This way, it is possible to get information about the state of the host from the `AppDomainManager`; see for example [`HostContext::GetThreadCount()`](https://github.com/ldematte/SimpleHost/blob/master/HostContext.cpp#L58)

(*) this is possible because we create threads and AppDomains in a controlled way: the only AppDomains allowed are our sandbox; each snippet runs in one sandbox; we create a thread before creating the new AppDomain, and therefore there is a controlled Thread-AppDomain relationship. 
It is indirect, but it is the only documented way (there are undocumented way that use the Thread TEB to get the current AppDomain for each thread, but we tried to avoid that).

(**) Managed-unmanaged interactions, especially between hosting code (`AppDomainManager` and HostContext, for example) can be tricky. We solved it by using the simplest form of COM interop: direct call of `IUnkwnown` interface pointers. We made the `AppDomainManager` COM visible, and we made the `HostContext` a coclass. Special care is needed when there are multiple managed-unmanaged transitions (see next section).

##### AppDomainManager 

The main host manager, `HostCtrl`, has a callback method used by the CLR to inform the host that a AppDomain has been created. The first one is the default AppDomain; we save a pointer to its interface, and use it to call back into managed code (to request the execution of a snippet, for example, or to notify some external -not initiated by our code- events). Notice that you can call into managed code only at specific times (e.g. you cannot call it from inside one of the manager methods, like `CreateTask`, as this will create an unbalanced sequence of managed-unmanaged transactions).

For this we implemented a method on `HostContext` ([`HostContext::GetLastMessage`](https://github.com/ldematte/HostedPumpkin/blob/master/HostContext.h)), that acts as a classical event pump: you call it, it blocks, and returns either after a timeout (0 to Infinite) or when an event is present. So the managed code can call this method, and receive notifications back from the unmanaged portion of the Host safely.

The `AppDomainManager` for the default AppDomain creates the [`DomainPool`](https://github.com/ldematte/HostedPumpkin/blob/master/SimpleHostRuntime/DomainPool.cs), which creates, maintains and check the status of a pool of threads, each of which immediately creates and enter a new sandbox AppDomain, and waits for a snippet to execute.

##### Escalation Policy

We use escalation policies to create our Error state behaviour. 
The defaults are not good in our case: some operations do not time out at all, or the default behaviour is to take no action; other operations take a safer road, which is too extreme in our case. For example, the default policy for a uncaught exception in a thread is to bring down the whole process, and we surely do not want that.

The hosting API offers a one-level escalation: you say "try that; if it works, OK. Otherwise, do something else".

As an example, let's consider 
For example, consider what happens if we want to terminate a snippet which is taking too much CPU: we abort its thread. 

The thread may not exit cleanly; by default an abort that the finally blocks (and the finalizers) get a chance to run. Suppose one goes into an infinite loop (this is a scenario I tested, see the [TestApplication project](https://github.com/ldematte/HostedPumpkin/blob/master/TestApplication/Program.cs))

    void Snippet() {
      try {
        while(true) {
          ++i;
        }
      }
      finally() {
        while(true);
      }
    }

With the default runtime policy, that thread will never be aborted.
The solution is to chain a series of escalation policies:

 1. use `SetTimeoutAndAction` on `ICLRPolicyManager` (to specify, for example, appDomainUnload -> 5 seconds -> (timeout?) rudeAppDomainUnload (which terminates all the threads)
2. Register a `IHostPolicyManager`, and get notifications of these events (through the `OnError` callback), so we can mark the relevant AppDomain as Zombie, and decide if we need further escalation.

The escalation policies are similar to what SQL Server implements:

- failure to get a resource (OOM) results in the unload of the snippet AppDomain
- leaving an orphaned lock results in the unload of the snippet AppDomain
- a StackOverflow leads to the unload of the AppDomain

with these escalation:

- (after a timeout) Thread Abort -> Rude Abort
	- setting the finalizer timeout
- (after a timeout) AppDomain unload -> Rude unload
- (immediately) Thread Rude Abort in critical region -> Rude AppDomain abort

Rude AppDomain aborts put the AppDomain in a "zombie" state: unload was not completely successful, and resources may have leaked.
We keep a counter (in the [HostContext](https://github.com/ldematte/SimpleHost/blob/master/HostContext.cpp#L85)) and, if the counter reach a too high number, we kill and recycle the process.

Solution check
--------------

#### Permission control

Permission control let us answer the first demand (*Which "security" do we want to provide?*), as well as points 3., 5., 6., 7. of the second requirement (*Which "resources" we want to control?*).

It is obtained through three different mechanisms:

- Explicit PermissionSet for the snippet AppDomains (see [AppDomainHelpers.cs](https://github.com/ldematte/HostedPumpkin/blob/master/SimpleHostRuntime/AppDomainHelpers.cs))
- Inside the Host, using the [ClrHostProtectionManager](https://github.com/ldematte/HostedPumpkin/blob/master/main.cpp)
- At submission time, by using Cecil [CheckAssemblyAgainstWhitelist](https://github.com/ldematte/HostedPumpkin/blob/master/Pumpkin.Submission/SnippetCompiler.cs) with a whitelist of types, assemblies and methods, for a finer-grained control.

Also, we forbid snippet threads to adjust their priority.
Fortunately, Thread.SetPriority is wired to one of the host interfaces([IHostTask::SetPriority](https://github.com/ldematte/SimpleHost/blob/master/Threading/Task.cpp#L96)), and we prevent them from getting high priority.

#### Resources control

Resource control is obtained through a combination of managed code, **AppDomain sandboxing**, **Escalation policies** and tracking inside the **memory and concurrency managers** (`IHostMemoryManager` and `IHostTaskManager`).

AppDomains create data boundaries, but not execution boundaries. 
Controlling memory inside an AppDomain is easy, controlling execution is more.. difficult: since AppDomain sandboxing offers little nothing out of the box, what we do is to have a spawn a separate thread (with priority higher than the snippet threads, which we limit thanks to the managers) that checks the snippet status periodically.

In particular, it detects if a snippet is running for too long (a possible extension: check also if it used too much CPU time) and if so it aborts its thread. 
Inside the managed code: each thread in the domain pool catches the `ThreadAbortException`, resets it (note: snippets cannot do that, CAS polices we set up forbid this operation), records the event and then proceed normally, checking the status.

If the thread does not exits cleanly, we rely on **escalation policies** (see above).
Otherwise, we check if there are any "runaway" threads: threads created by the snippet that are still running. 
If there are any, we need to unload the AppDomain in which the snippet was executing. This will abort all the remaining threads in the domain.  Again, escalation policies will let us handle the situation in case of a thread that does not want to go down; furthermore, our Host implements the [`IHostPolicyManager`](https://github.com/ldematte/HostedPumpkin/blob/master/PolicyManager.h), to get notifications when such an escalation happened.
This is important, as we use this information to know how many partially unloaded AppDomains are in our process: they leak memory. See the next section for more details.

**Memory and concurrency managers** (`IHostMemoryManager` and `IHostTaskManager`) are notified of thread creation, so you can monitor it; that's how we know that too much memory is used, or too many threads are created, or which (and how many) threads are created inside an AppDomain.

A final word on "too many threads are created": if a thread is spawning "fork bombs", and each is running at full speed, they could tear down enough CPU to block the supervisor-kill thread for a long time. We mitigate this problem by scheduling the admin threads at an higher priority relative to the user code threads; the windows scheduler is priority-based, so our supervisor will always preempt the execution of the snippet threads.


#### Performances

Reusing and sharing processes and threads has a big advantage in terms of startup speed, resource usage and scalability. The price is that you lose the nice OS-provided isolation (but the CLR, AppDomains and our own checks should suffice) and OS-provided cleanup. 

We keep a pool of AppDomains, running each snippet inside one of the available ones, so we even amortize the cost of AppDomain creation, not only the cost of Process or Thread creation.


#### Safety

Safety is obtained through three mechanism:

- AppDomain sandboxing, with:
	- The built-in safety of the CLR: types, boundaries.
	- CAS policies
- Assembly/types whitelisting
- The CLR Host protection manager ([`ICLRHostProtectionManager`](https://github.com/ldematte/HostedPumpkin/blob/6583d6c262d34870183abc4d961e0822228c4f17/main.cpp#L166))

Also, in our code we take extra steps to ensure that a malicious snippet cannot interfere with our own code, or with other processes (no access to global, named synchronization primitives, for example).

Problematic snippets will be detected and the domain they are currently hosted into will be unloaded, reducing the chance that they can impact our code or other snippets.

On unload, if something wrong happens (the unload has problem too) we flag the snippet as “really troublesome”, and then we leak the domain. We keep an eye on the memory and on the number of “zombies”, and when a threshold is reached, cycle the process.

This is the same approach followed by IIS and by the SQL Server CLR, so we are pretty confident that it will be safe. 
For example, IIS unloads the domain of a webapp after a (default of) 20 minutes of inactivity. By default, IIS will recycle the process when it reaches some limit, and it will also start a new one and "phase over" all incoming requests until the old one is unused, in order to to minimize disruption.

So, we structured the core project as a pair of process: a monitor and a (pool of) workers. The workers are CLR hosts, and they keep track of the snippet “execution” state in each AppDomain (states can be: `Waiting, Running, Finished, Timed-out, Error, Zombie`).

The normal cycle (`Waiting -> Running -> Finished`) leads to a “free” AppDomain, that can be reused to run a new snippet (after running a GC and assuring that no memory was leaked/is still allocated in the AppDomain). 
The AppDomain state cycles back (`Finished -> Waiting`)

In case of an error (unhandled exception, memory limit, thread/execution limit, ...) the snippet transitions to an error state (`Running -> Error` or `Running -> Timed-out)`. We then try to unload the AppDomain. If it fails, the AppDomain is marked as `Zombie`.

As a safety measure, when the snippet exit is NOT clean, we always unload the domain. We also signal the snippet as `medium critical`.
IF the unloading fails and we have to mark the domain as `Zombie`, and we signal the snippet as a `highly critical`.
If we have too many AppDomains in a `Zombie` state, we recycle (kill and restart) the Host process to free all memory.

We can think of three policies for snippets flagged as `critical` in the DB:

- disallow their execution,
- signal them for review
- execute them in a completely separate process, and exit the process immediately after the snippet execution to be sure to free all resources.

