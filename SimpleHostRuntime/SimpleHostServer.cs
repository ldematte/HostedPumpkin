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

namespace Pumpkin.Web {
   class SimpleHostServer {

      public static void StartListening() {
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

      private static async Task WaitForConnection(Socket listener) {
         while (true) {
            // Start an asynchronous socket to listen for connections.
            Console.WriteLine("Waiting for a connection...");
            Socket socket = await listener.AcceptAsync();
            Console.WriteLine("Connected");
            HandleConnection(socket);          
         }
      }

      private static Random random = new Random();

      private static Dictionary<int, TaskCompletionSource<string>> submissionMap = new Dictionary<int, TaskCompletionSource<string>>();

      private static Task<string> DoStuffAndReturnWhenIdFinished(int id) {
         var tcs = new TaskCompletionSource<string>();
         submissionMap.Add(id, tcs);
         // Post in queue, and return immediately
         // The pool/deque will "call us back" (set the status) when finished

         ThreadPool.QueueUserWorkItem((arg) => {

            Thread.Sleep(1000);
            tcs.SetResult("Done " + id);

         });

         return tcs.Task;
      }

      public static async void HandleConnection(Socket socket) {
         try {
            string command = "";
            while (socket.Connected && command.IndexOf("<EOF>") == -1) {
               command = await socket.ReceiveAsync();

               int submissionId = random.Next();

               // Do stuff
               string result = await DoStuffAndReturnWhenIdFinished(submissionId);

               Debug.Assert(result.Equals("Done " + submissionId));

               await socket.SendAsync(result, true);
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
