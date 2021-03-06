﻿HostedPumpkin
=============

Proof-Of-Concept (POC) for the submission, compilation and execution of C# code snippets.

The solution is composed by the following sub-projects:

- The main project is a unmanaged C++ project, which hosts the CLR using the Hosting API. The Host uses two helper assemblies: 
 - a custom [AppDomainManager](https://msdn.microsoft.com/en-us/library/system.appdomainmanager(v=vs.110).aspx) (project _SimpleHostRuntime_)
 - a set of common classes (project _Pumpkin.Monitor_) loaded with the snippet assembly
- The _Pumpkin.Submission_ project compiles snippets using the [CodeDOM compiler](https://msdn.microsoft.com/en-us/library/system.codedom.compiler(v=vs.110).aspx) (i.e. going through `csc.exe`), then serializes the assemblies.
- The _Pumpkin.Data_ project provides supporting functions, for example it helps in storing and retrieving compiled snippets in/from a DB
- The _Pumpkin.Web_ project is a very simple ASP.NET MVC project, with pages for submitting a new snippet, listing all the snippets and execute them.

The goal of this POC is to take some C#, compile it, run it. Easy :) 
More precisely, we want to execute third-party code (a “snippet”) in a safe, reliable and efficient way.

The various projects in the solution collaborate as depicted in this architectural diagram:

![](https://github.com/ldematte/SimpleHost/blob/master/resources/Components.png)

More in detail; the implementation separates the phases of compiling a new snippet ("submitting" it) and of executing it. 
The reasoning behind this choice was that the first is usually a one-time operation; a user write some demo code in a snippet and submits it. The MVC controller inside _Pumpkin.Web_ takes it, compiles it using classes from _Pumpkin.Submission_ and stores it in the DB with the help of _Pumpkin.Data_.

Then when other users see the snippet, they hit the "Go" button on the snippet list page, and the system runs it: the page sends a POST to another controller method in _Pumpkin.Web_, which in turn sends the snippet id to the _SimpleHostRuntime_ running inside the Host; the host loads it from the DB with the help of _Pumpkin.Data_ and executes it:

	// called when a new snippet is created
	[HttpPost]
	public ActionResult SubmitSnippet(/*snippet code params*/) {
	    // Take the code, using statements, etc. Put them together and compile it
	    var compiledSnippet = Pumpkin.SnippetCompiler.CompileWithCSC(...);
	    // the compilation produces an assembly as a byte[]
	    if (compiledSnippet.success) {
	      //Save the byte[] into the DB
	      repository.Save(compiledSnippet);
	      return new HttpStatusCodeResult(204);
	    }
	}
---
	// Called when we want to run a previously compiled snippet
	[HttpPost]
	public async Task<ActionResult> RunSnippetAsync(String snippetId) {
	   return Json(await HostConnector.RunSnippetAsync(snippetId));
	}
	public static async Task<SnippetResult> RunSnippetAsync(string snippetId) {
	   var socket = GetSocket();
	   await socket.SendAsync(snippetId, true); 
	   // Send the snippet ID to the host.
	   // The host takes the snippet ID, loads the snippet assembly from the DB,
	   // executes it and returns the result (as a JSON)(**)
	   string s = await socket.ReceiveAsync();
	   return JsonConvert.DeserializeObject<SnippetResult>(s);
	}
---
	// (**) Inside the host, in the HostServer class:
	var command = await socket.ReceiveAsync();
	var snippetId = Guid.Parse(command);
	var snippetInfo = repository.Get(snippetId);
	queue.SubmitSnippet(snippetInfo, tcs);


The various bits can be re-arranged at will: a good idea would be to run the snippet just after compilation/submission, maybe in an "isolated" Host: a Host that uses a single AppDomain (not a pool, as the regular ones), where the snippet can fail, timeout, throw... we could "test" it upon submission, just after compilation, so we know if the snippet can be accepted in the system. Or if there are errors, we can put it in a review queue, or reject it, or use the "Exceptional queue" in the picture.



### Usage

The main project, the Host, can be used in a "standalone" way, to run a series of tests or a single snippet:

      USAGE:

         SimpleHost.exe  [-p <int>] [-d <string>] [-m <string>] [-c <string>] [-a
                         <string>] -v <string> [--] [--version] [-h]


      Where:

         -p <int>,  --port <int>
           The port on which this Host will listen for snippet execution requests

         -d <string>,  --database <string>
           The path of the SQL CE database that holds the snippets

         -m <string>,  --method <string>
           The method to invoke

         -c <string>,  --type <string>
           The type name which contains the method to invoke

         -a <string>,  --assembly <string>
           The assembly file name

         -v <string>,  --clrversion <string>
           (required)  CLR version to use

         --,  --ignore_rest
           Ignores the rest of the labeled arguments following this flag.

         --version
           Displays version information and exits.

         -h,  --help
           Displays usage information and exits.


To run a single snippet: pass the `-t (--test)` option, with the `-a (--assembly)`, `-c (--type)` and, optionally, the `-m (--method)` options.
They are used to specify the method to run, the class (FullName) that contains the method, and the assembly file (`.dll` or `.exe`), where the class is implemented.

Example:
    
    -v v4.0.30319 -t -a "$(LocalDebuggerWorkingDirectory)TestApplication\bin\Debug\TestApplication.exe" -c TestApplication.Program -m SnippetTest12

If the `--method` option is omitted, all methods which name starts with `SnippetTest` will be run. 

Example:
     
    -v v4.0.30319 -t -a "$(LocalDebuggerWorkingDirectory)TestApplication\bin\Debug\TestApplication.exe" -c TestApplication.Program

Otherwise, the `-d (--database)` option is required. This option indicates the full path to the SQL Compact database used to store the snippets.
The Host starts, listen on the port specified by `-p (--port)` (default: 4321), and execute Snippets as they are requested, loading them from the database.

Example:

    -v v4.0.30319 -d "$(SolutionDir)Pumpkin.Web\App_Data\Snippets.sdf"

You must also specify a CLR/.NET version which is installed on your system (e.g. `-v v4.0.30319`).
The available versions are those listed under `%WINDIR%\Microsoft.NET\Framework`.

### TODO

- <del>Set THREAD_PRIORITY_ABOVE_NORMAL for supervisor threads</del>
- Extra safety: killing the supervisor (watchdog) thread brings process down
- <del>Same for server thread</del>
- No named sync primitives (i.e.: forbid `Mutex` usage)
   - (or better: decorate the name with the snippet GUID)
- <del>Choose C#/.NET version during snippet submission, record it.</del>
   - Get the list of available .NET SDKs using the Hosting API (from the Host)
- Choose the references from a list (during submission, for compilation)
      - Same "whitelist" used by Cecil post-processing
- <del>Allow definition of static methods and classes</del>
- Supervisor: spawn two (N) hosts, maintain the count.
- Use a distributed queue (e.g. Redis) for snippet submission/results, instead of plain sockets
   - Move the "server" thread from managed to unmanaged, to improve stability and error handling in case of FEEE (CLR unload)
- Use a separate, "single-run" host for "problematic" snippets.
     - Host can be run/configured in "single mode": one snippet, execute and exit.
- Use a separate host for each .NET version
- Use the [new pooling API](https://msdn.microsoft.com/en-us/library/windows/desktop/ms686760.aspx) for better handling/managing ThreadPool threads
- Finish implementation of the various `Console.WriteLine` overloads in `Pumpkin.Monitor`
- <del>Measure complete round-trip time</del>
- **TEST TEST TEST!**
- ***BUGS BUGS BUGS!***

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
 - deny (or handle in a sensible way) access to named kernel objects (e.g. named mutexes.. you do not want some casual interaction with other mutexes!)
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

When a snippet is submitted and compiled, we could pre-process its source code to recognize patterns, remove "unsafe" code, and inject control "probes" inside the code.
These "probes" can then be used to monitor resource usage (i.e. allocation, CPU usage, thread creation, ...). 
Classes that we deem unsafe can either be forbidden (raising an error and rejecting the snippet), or replaced.

We considered [Roslyn](http://roslyn.codeplex.com/), the new (and open-source) C# compiler, but discarded it, as one of the requirements was to let the user choose different, older, C# versions.
We then used the `CodeDOM` compiler, which is just a managed wrapper around the actual `csc.exe` (of which there are multiple instances on a machine, one for each SDK version).

### Control at execution time

#### AppDomain sandboxing 

Using the protection given by AppDomains is the bare minimum: we want to run each snippet in its own [separate AppDomain sandbox](https://msdn.microsoft.com/en-us/library/bb763046(v=vs.110).aspx), with a custom `PermissionSet`, starting with an (almost) empty one. 
AppDomain sandboxing helps with the security aspect, but not for the resource control, nor does it give enough protection in case of faults (for example, thread aborts, unhandled exceptions, failed AppDomain unloads, ...)

#### Hosting the CLR

Building a custom CLR host means running the CLR (and the snippets) inside our own executable, which is notified of several events and acts as a proxy between the managed code and several unmanaged resources, like tasks (threads) and memory.

Microsoft provides an [Hosting API](http://msdn.microsoft.com/en-us/library/dd380850(v=vs.110).aspx), which is used for very similar purposes (protection and enforcement) by ASP.NET and SQL Server.
For example, you can control and replace all the native implementations of ["task-related" functions](http://msdn.microsoft.com/en-us/library/ms164562(v=vs.110).aspx); SQL Server uses this API to run .NET code using his custom scheduler.

#### Debugging and CLR Profiling API

These API (the [Managed Debugging API](https://msdn.microsoft.com/en-us/library/vstudio/bb384605(v=vs.90).aspx), [ClrMD](http://blogs.msdn.com/b/dotnet/archive/2013/05/01/net-crash-dump-and-live-process-inspection.aspx?) and the CLR Profiling API (see [1](http://msdn.microsoft.com/en-us/library/bb384493.aspx) and  [2](http://msdn.microsoft.com/en-us/library/dd380851(v=vs.110).aspx)) give plenty of control: you can "see" and "do" everything that a debugger sees or does.

This includes step-by-step execution, notifications about thread creation and destruction, assembly and DLL load,  on-the-fly IL rewriting (you are notified when a method is JIT-ed and you can modify the IL stream before JIT); you  effectively gain complete control on the debuggee.

The POC
-------

We divided the solution in two main (logical) steps: *snippet submission* and *snippet execution*.
As we have discussed above, a snippet is compiled using `csc.exe` (through the `CodeDOM` classes); however, there is more going on during this step.

### Snippet submission

A first attempt to tackle the whole problem was made using only managed resources; this approach was based on isolation through AppDomains, while supervision on resource usage was done through some "probes", injected at submission time. Methods which could have been problematic, for example all constructors, all thread functions/delegates, etc. where patched (at IL level) in order to add some "fail-safes" (global `try`-`catch` to avoid unhandled exceptions, for example) and to add code for monitoring resource usage.

In this first POC the _snippet submission_ part was considerably prominent: execution was protected by either .NET security (AppDomain sandbox) or our own injected code (for the resource usage part).

Since compilation with `CodeDOM` is a black box, and we could not use Roslyn due to other requirements, we used [Mono.Cecil](https://github.com/jbevain/cecil) to inspect and patch the IL code.

We then went for a different approach (adding Hosting into the mix), but the submission part still uses Cecil to inspect and rewrite code (for type/method/assembly whitelistin, or to substitute some types and functions with our own; for example, we use it to "redirect" calls to `Console.WriteLine` to a memory buffer).

###Snippet execution

For the snippet execution part, we use a combination of AppDomains for security and isolation, and CLR Hosting to track and control execution.
AppDomains are used, to the degree in which this is possible, to isolate the snippet: two snippets cannot interact in any way, and a misbehaving snippet should not affect others. 

They are also a nice compromise in terms of balancing performance/scalability and safety/control/isolation.

Reusing and sharing processes and threads has a big advantage in terms of startup speed, resource usage and scalability. The price is that you lose the nice OS-provided isolation  and OS-provided cleanup.

We implemented an AppDomain pool, in which each AppDomain follows a series of policies to decide if it can be reused for another snippet execution, or if it needs to be recycled (unloaded and replaced by a new one) and to track resource usage and faults.

Resource tracking is done by the Host interfaces, with the help of a `HostContext` class; faults are detected using a combination of managed code inside the the `AppDomainManager` and in the Host, by using escalation policies (more on these policies later).

![](https://github.com/ldematte/SimpleHost/blob/master/resources/ThreadFlow.png)


##### Host interfaces, HostContext 

In order to gain as much insight as possible on the various events happening inside the CLR during the execution of our snippets, we implemented all the available Host Manager interfaces (see [1](https://msdn.microsoft.com/en-us/library/ms404385(v=vs.110).aspx) and [2](https://msdn.microsoft.com/en-us/magazine/cc163567.aspx)). 

If the host implements a manager, the CLR does not create the associated resource (thread, memory, etc.) by itself, but offloads its management to the Host. So we intercepted virtually all calls that the code could make to the CLR and "re-implement" them with unmanaged primitives. 

In short, the CLR inside is full of places where it either asks the host to provide some functionality, or do it directly if no host is provided. An example from (managed) thread creation:

    IHostTaskManager *hostTaskManager = CorHost2::GetHostTaskManager();
    if (hostTaskManager) {
       hostTaskManager->GetCurrentTask(&curHostTask);
    }

The interfaces provided by the Hosting API are implemented in the more straightforward way possible, with some exceptions. The goal is to mimic really closely what the "standard" (non-hosted) version of the CLR does. Obviously, we add code to record what is going on: the goal is to collect resource usage, per snippet, in order to cap them, or  prevent some events from happening (setting thread priority above normal? No-no!) and react to problems (a snippet is creating thousands of threads? Stop it and notify the `AppDomainManager`: it will try to Abort it and then (eventually) escalate the issue).

All the classes implementing the Host Manager interfaces receive a reference to a `HostContext` class, which records information about resources in order to track them. It tracks `AppDomain <-> Thread` relationships(*), `Managed thread` (called `CLRTask`s in the Hosting API) `<-> Native thread` relationships (called `HostTask`s in the Hosting API), `Parent <-> Child` relationships between threads, `AppDomain <-> Memory`, ...

`HostContext` is a coclass, so it can be accessed from managed code(**). This way, it is possible to get information about the state of the host from the `AppDomainManager`; see for example [`HostContext::GetThreadCount()`](https://github.com/ldematte/SimpleHost/blob/master/HostContext.cpp#L58).

(*) this is possible because we create threads and AppDomains in a controlled way: the only AppDomains allowed are our sandboxes; each snippet runs in one sandbox; we create a thread before creating the new AppDomain, and therefore there is a controlled `Thread <-> AppDomain` relationship. 
It is indirect, but it is the only documented way to get this information (there are undocumented way that use the Thread TEB to get the current AppDomain for each thread, but we preferred to not rely on undocumented features).

(**) Managed-unmanaged interactions, especially between hosting code (`AppDomainManager` and HostContext, for example) can be tricky. We solved it by using the simplest form of COM interop: direct call of `IUnkwnown` interface pointers. We made the `AppDomainManager` COM visible, and we made the `HostContext` a coclass. Special care is needed when there are multiple managed-unmanaged transitions (see next section).

##### AppDomainManager 

The main host manager, `HostCtrl`, has a callback method used by the CLR to inform the host that a AppDomain has been created. The first one is the default AppDomain; we save a pointer to its interface, and use it to call back into managed code (to request the execution of a snippet, for example, or to notify some external -not initiated by our code- events). Notice that you can call into managed code only at specific times (e.g. you cannot call it from inside one of the manager methods, like `CreateTask`, as this will create an unbalanced sequence of managed-unmanaged transactions).

For this reason, we implemented a method on `HostContext` ([`HostContext::GetLastMessage`](https://github.com/ldematte/HostedPumpkin/blob/master/HostContext.h)), that acts as a classical event pump: you call it (from managed code), it blocks, and returns either after a timeout (0 to Infinite) or when an event is present - if you ever did Win32 programming that should sound familiar. So out `AppDomainManager` (the only managed "Manager" in the CLR Hosting framework) can call this method, block, and receive notifications back from the unmanaged portion of the Host in a safe way.

The `AppDomainManager` for the default AppDomain creates the [`DomainPool`](https://github.com/ldematte/HostedPumpkin/blob/master/SimpleHostRuntime/DomainPool.cs), which creates, maintains and check the status of a pool of threads, each of which immediately creates and enter a new sandbox AppDomain, and waits for a snippet to execute.

##### Escalation Policy

We use escalation policies to create our error state behaviour. 
The defaults are not good in our case: some operations do not time out at all, or the default behaviour is to take no action; other operations take a safer road, which is too extreme in our case. For example, the default policy for an uncaught exception in a thread is to bring down the whole process, and we surely do not want that.

The hosting API offers a one-level escalation: you say "try that; if it works, OK. Otherwise, do something else".

As an example, let's consider what happens if we want to terminate a snippet which is taking too much CPU: we abort its thread. 

The thread may not exit cleanly; by default, after a `Thread.Abort`, the finally blocks and the finalizers get a chance to run. Suppose one goes into an infinite loop (this is a scenario I tested, see the [TestApplication project](https://github.com/ldematte/HostedPumpkin/blob/master/TestApplication/Program.cs))

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
Fortunately, `Thread.SetPriority` is wired to one of the host interfaces ([IHostTask::SetPriority](https://github.com/ldematte/SimpleHost/blob/master/Threading/Task.cpp#L96)), and we prevent snippets from getting a high priority, simply ignoring the request (this is allowed by the Hosting API contract).

#### Resources control

Resource control is obtained through a combination of managed code, **AppDomain sandboxing**, **Escalation policies** and tracking inside the **memory and concurrency managers** (`IHostMemoryManager` and `IHostTaskManager`).

AppDomains create data boundaries, but not execution boundaries. 
Controlling memory inside an AppDomain is easy, controlling execution is more... difficult: since AppDomain sandboxing offers little nothing out of the box, what we do is to spawn a separate thread (with priority higher than the snippet threads, which we limit thanks to the managers) that checks the snippet status periodically.

In particular, this "watchdog" thread detects if a snippet is running for too long (a possible extension: check also if it used too much CPU time) and if so it aborts its thread. 
Inside the managed code: each thread in the domain pool catches the `ThreadAbortException`, resets it (note: snippets cannot do that, CAS polices we set up forbid this operation), records the event and then proceed normally, checking the status.

If the thread does not exits cleanly, we rely on **escalation policies** (see above).
Otherwise, we check if there are any "runaway" threads: threads created by the snippet that are still running. 
If there are any, we need to unload the AppDomain in which the snippet was executing. This will abort all the remaining threads in the domain.  Again, escalation policies will let us handle the situation in case of a thread that does not want to go down; furthermore, our Host implements the [`IHostPolicyManager`](https://github.com/ldematte/HostedPumpkin/blob/master/PolicyManager.h), to get notifications when such an escalation happened.
This is important, as we use this information to know how many partially unloaded AppDomains are in our process: they leak memory. See the next section for more details.

**Memory and concurrency managers** (`IHostMemoryManager` and `IHostTaskManager`) are notified of thread creation, so you can monitor it; that's how we know that too much memory is used, or too many threads are created, or which (and how many) threads are created inside an AppDomain.

A final word on "too many threads are created": if a thread is spawning "fork bombs", and each is running at full speed, they could tear down enough CPU to block the supervisor-kill thread for a long time. We mitigate this problem by scheduling the admin threads at an higher priority relative to the user code threads; the windows scheduler is priority-based, so our supervisor will always preempt the execution of the snippet threads.


#### Performances

Reusing and sharing processes and threads has a big advantage in terms of startup speed, resource usage and scalability. The price is that you lose the nice OS-provided isolation (but the CLR, AppDomains and our own checks should suffice), as well as OS-provided cleanup. 

We keep a pool of AppDomains, and we run each snippet inside one of the available AppDomains, so we even amortize the cost of AppDomain creation, not only the cost of Process or Thread creation.

#### Safety

Safety is obtained through three mechanism:

- AppDomain sandboxing, with:
	- The built-in safety of the CLR: types, boundaries.
	- CAS policies
- Assembly/types whitelisting
- The CLR Host protection manager ([`ICLRHostProtectionManager`](https://github.com/ldematte/HostedPumpkin/blob/6583d6c262d34870183abc4d961e0822228c4f17/main.cpp#L166))

Also, in our code we take extra steps to ensure that a malicious snippet cannot interfere with our own code, or with other processes (no access to global, named synchronization primitives, for example).

Problematic snippets will be detected and the domain they are currently hosted into will be unloaded, reducing the chance that they can impact our code or other snippets.

On unload, if something wrong happens (the unload has problems too) we flag the snippet as “really troublesome”, and then we leak the domain. We keep an eye on the memory and on the number of “zombies”, and when a threshold is reached, we cycle (kill and restart) the host process.

This is the same approach followed by IIS and by the SQL Server CLR, so we are pretty confident that it will be safe. 
For example, IIS unloads the domain of a webapp after a (default of) 20 minutes of inactivity. By default, IIS will recycle the process when it reaches some limit, and it will also start a new one and "phase over" all incoming requests until the old one is unused, in order to to minimize disruption.

This is the reason why we structured the core project as a pair of processes: a **Supervisor**, and a (pool of) workers. The worker(s) are **CLR hosts** (see the first picture of this document). Each host keeps track of the snippet state in each AppDomain (states can be: `Waiting, Running, Finished, Timed-out, Error, Zombie`).

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

These policies are not currently implemented, but can be easily added to the solution.
