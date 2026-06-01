using System;
using System.Windows.Forms;
using S3StorageClient.Forms;

namespace S3StorageClient
{
    internal static class Program
    {
        public static Services.ApiService ApiService { get; } = new();
        public static Services.AuthService AuthService { get; } = new(ApiService);

        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            // Restore server URL from settings
            var serverUrl = Properties.Settings.Default.ServerUrl;
            if (!string.IsNullOrEmpty(serverUrl))
            {
                ApiService.SetBaseUrl(serverUrl);
            }

            // Try to restore session
            if (AuthService.TryRestoreSession())
            {
                Application.Run(new MainForm());
            }
            else
            {
                Application.Run(new LoginForm());
            }
        }
    }
}