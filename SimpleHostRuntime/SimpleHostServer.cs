using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using System.Diagnostics;
using System.Threading;

using Pumpkin.Data;
using Newtonsoft.Json;

namespace SimpleHostRuntime {
   public class SimpleHostServer {

      private readonly SnippetDataRepository repository;
      private readonly DomainPool domainPool;

      public SimpleHostServer(SnippetDataRepository repository, DomainPool domainPool) {
         this.repository = repository;
         this.domainPool = domainPool;
      }

      public void StartListening() {
         // Establish the local endpoint for the socket.
         // The DNS name of the computer
         // IPHostEntry ipHostInfo = Dns.GetHostEntry(Dns.GetHostName());
         // IPAddress ipAddress = ipHostInfo.AddressList[0];
         IPAddress ipAddress = IPAddress.Loopback;
         IPEndPoint localEndPoint = new IPEndPoint(ipAddress, 4321);

         // Create a TCP/IP socket.
         Socket listener = new Socket(ipAddress.AddressFamily,
             SocketType.Stream, ProtocolType.Tcp);

         // Bind the socket to the local endpoint and listen for incoming connections.
         try {
            listener.Bind(localEndPoint);
            listener.Listen(100);

            WaitForConnection(listener).Wait();
         }
         catch (Exception e) {
            Console.WriteLine(e.ToString());
         }

         Console.WriteLine("\nPress ENTER to continue...");
         Console.Read();
      }

      private async Task WaitForConnection(Socket listener) {
         while (true) {
            // Start an asynchronous socket to listen for connections.
            Debug.WriteLine("Waiting for a connection...");
            Socket socket = await listener.AcceptAsync();
            Debug.WriteLine("Connected");
            HandleConnection(socket);          
         }
      }

      private Task<SnippetResult> DoStuffAndReturnWhenIdFinished(Guid snippetId) {
         // Use a TCS for a Task without a "thread"
         // http://blogs.msdn.com/b/pfxteam/archive/2009/06/02/9685804.aspx
         var tcs = new TaskCompletionSource<SnippetResult>();
         
         try {
            // Post in queue, and return immediately
            // The pool/deque will "call us back" (set the status) when finished
            var snippetData = repository.Get(snippetId);
            var snippetInfo = new SnippetInfo() {
                  assemblyFile = snippetData.AssemblyBytes,
                  mainTypeName = SnippetData.SnippetTypeName,
                  methodName = SnippetData.SnippetMethodName,
                  submissionId = Guid.NewGuid().ToString()
               };

            domainPool.SubmitSnippet(snippetInfo, tcs);
         }
         catch (Exception ex) {
            tcs.TrySetException(ex);
         }
         // Return immediately, with the task we can await on
         return tcs.Task;
      }

      public async void HandleConnection(Socket socket) {
         try {
            while (socket.Connected) {
               try {
                  var command = await socket.ReceiveAsync();
                  if (command.IndexOf("<EOF>") != -1) {
                     break;
                  }
                  var snippetId = Guid.Parse(command);
                  var result = await DoStuffAndReturnWhenIdFinished(snippetId);
                  
                  var newStatus = result.status.ToHealth();
                  if (newStatus != SnippetHealth.Unknown)
                     repository.UpdateStatus(snippetId, newStatus);

                  var jsonResult = JsonConvert.SerializeObject(result);
                  var sentBytes = await socket.SendAsync(jsonResult, true);
                  Debug.WriteLine("Sent " + sentBytes + " bytes");
               }
               catch (Exception ex) {
                  Debug.WriteLine("HandleConnection Error: " + ex.Message);
                  Debug.WriteLine(ex.StackTrace);
               }
            }
         }
         finally {
            socket.Shutdown(SocketShutdown.Both);
            socket.Close();
         }
      }
   }
}
