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
   class SimpleHostServer {

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
            Console.WriteLine("Waiting for a connection...");
            Socket socket = await listener.AcceptAsync();
            Console.WriteLine("Connected");
            HandleConnection(socket);          
         }
      }

      private Task<SnippetResult> DoStuffAndReturnWhenIdFinished(string snippetId) {
         // Use a TCS for a Task without a "thread"
         var tcs = new TaskCompletionSource<SnippetResult>();
         
         try {
            // Post in queue, and return immediately
            // The pool/deque will "call us back" (set the status) when finished
            var snippetData = repository.Get(Guid.Parse(snippetId));
            var snippetInfo = new SnippetInfo() {
                  assemblyFile = snippetData.AssembyBytes,
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
               var command = await socket.ReceiveAsync();
               if (command.IndexOf("<EOF>") != -1) {
                  break;
               }

               var result = await DoStuffAndReturnWhenIdFinished(command);
               var jsonResult = JsonConvert.SerializeObject(result);
               await socket.SendAsync(jsonResult, true);
            }
         }
         catch (Exception ex) {
            Console.WriteLine("Error: " + ex.Message);
         }
         finally {
            socket.Shutdown(SocketShutdown.Both);
            socket.Close();
         }
      }
   }
}
