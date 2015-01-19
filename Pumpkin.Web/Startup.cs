using Microsoft.Owin;
using Owin;

[assembly: OwinStartup(typeof(CallbackTest.Startup))]
namespace CallbackTest
{
    public class Startup
    {
        public void Configuration(IAppBuilder app)
        {
            app.MapSignalR();
        }
    }
} 