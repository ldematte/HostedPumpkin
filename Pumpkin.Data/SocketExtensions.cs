using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using System.Web;

namespace Pumpkin.Data {
   public static class SocketExtensions {

      // From http://blogs.msdn.com/b/pfxteam/archive/2011/12/15/10248293.aspx
      // Consider the other solution if we have many more requests AND we do not move
      // to another method (e.g.: a redis queue)
      public static Task<int> ReceiveAsync(this Socket socket, byte[] buffer,
         int offset, int size, SocketFlags socketFlags) {
         var tcs = new TaskCompletionSource<int>(socket);
         socket.BeginReceive(buffer, offset, size, socketFlags, iar => {
            var t = (TaskCompletionSource<int>)iar.AsyncState;
            var s = (Socket)t.Task.AsyncState;
            try { 
               t.TrySetResult(s.EndReceive(iar)); 
            }
            catch (Exception exc) { 
               t.TrySetException(exc); 
            }
         }, tcs);
         return tcs.Task;
      }

      public static Task<int> SendAsync(this Socket socket, byte[] buffer,
         int offset, int size, SocketFlags socketFlags) {
         var tcs = new TaskCompletionSource<int>(socket);
         socket.BeginSend(buffer, offset, size, socketFlags, iar => {
            var t = (TaskCompletionSource<int>)iar.AsyncState;
            var s = (Socket)t.Task.AsyncState;
            try { 
               t.TrySetResult(s.EndSend(iar)); 
            }
            catch (Exception exc) { 
               t.TrySetException(exc); 
            }
         }, tcs);
         return tcs.Task;
      }

      public static Task<int> SendAsync(this Socket socket, string s, bool terminateString) {
         byte[] buffer;
         if (terminateString)
            buffer = Encoding.Unicode.GetBytes(s + "\0");
         else
            buffer = Encoding.Unicode.GetBytes(s);

         return SendAsync(socket, buffer, 0, buffer.Length, SocketFlags.None);
      }

      class SocketStatus {
         public TaskCompletionSource<string> taskCompletionSource;
         public const int BufferSize = 1024;
         public byte[] buffer = new byte[BufferSize];
         public StringBuilder sb = new StringBuilder();

         public SocketStatus(Socket socket) {
            taskCompletionSource = new TaskCompletionSource<string>(socket);
         }
      }

      private static void ReceiveCallback(IAsyncResult iar) {
         var socketStatus = (SocketStatus)iar.AsyncState;
         var socket = (Socket)socketStatus.taskCompletionSource.Task.AsyncState;

         try {
            int read = socket.EndReceive(iar);

            // Socket closed, or read error
            if (read <= 0) {
               //All of the data has been read, so return it
               socketStatus.taskCompletionSource.TrySetResult(socketStatus.sb.ToString());
            }
            else {
               var s = Encoding.Unicode.GetString(socketStatus.buffer, 0, read);
               var finished = isFinished(socketStatus.buffer, read);               

               if (finished) {
                  socketStatus.sb.Append(s, 0, s.Length - 1);
                  socketStatus.taskCompletionSource.TrySetResult(socketStatus.sb.ToString());
               }
               else {
                  socketStatus.sb.Append(s);
                  socket.BeginReceive(socketStatus.buffer, 0, SocketStatus.BufferSize, 0,
                                      new AsyncCallback(ReceiveCallback), socketStatus);
               }
            }            
         }
         catch (Exception ex) {
            socketStatus.taskCompletionSource.TrySetException(ex);
         }
      }

      private static bool isFinished(byte[] buffer, int len) {
         //return (buffer[len - 1] == 10 || buffer[len - 1] == 13);
         // Unicode, c-style terminator
         return (len >= 2 && buffer[len - 2] == 0 && buffer[len - 1] == 0);
      }

      public static Task<string> ReceiveAsync(this Socket socket) {
         SocketStatus status = new SocketStatus(socket);

         socket.BeginReceive(status.buffer, 0, SocketStatus.BufferSize, 0, 
                                    new AsyncCallback(ReceiveCallback), status);

         return status.taskCompletionSource.Task;
         
         
      }

      public static Task<Socket> AcceptAsync(this Socket socket) {
         var taskCompletionSource = new TaskCompletionSource<Socket>();
         socket.BeginAccept(iar => {
            var listenerSocket = (Socket)iar.AsyncState;
            try {
               Socket s = listenerSocket.EndAccept(iar);
               taskCompletionSource.TrySetResult(s);
            }
            catch (Exception ex) {
               taskCompletionSource.TrySetException(ex);
            }
         }, socket);
         return taskCompletionSource.Task;
      }
   }
}