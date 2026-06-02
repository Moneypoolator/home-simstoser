using System;
using System.Windows;
using S3StorageClient.Properties;

namespace S3StorageClient
{
    public partial class App : Application
    {
        public static Services.ApiService ApiService { get; } = new();
        public static Services.AuthService AuthService { get; } = new(ApiService);

        private void Application_Startup(object sender, StartupEventArgs e)
        {
            // Restore server URL from settings
            var serverUrl = Settings.Default.ServerUrl;
            if (!string.IsNullOrEmpty(serverUrl))
            {
                ApiService.SetBaseUrl(serverUrl);
            }

            // Try to restore session
            if (AuthService.TryRestoreSession())
            {
                var mainWindow = new Views.MainWindow();
                mainWindow.Show();
            }
            else
            {
                var loginWindow = new Views.LoginWindow();
                loginWindow.Show();
            }
        }

        private void Application_DispatcherUnhandledException(object sender,
            System.Windows.Threading.DispatcherUnhandledExceptionEventArgs e)
        {
            MessageBox.Show($"An unexpected error occurred:\n{e.Exception.Message}",
                "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            e.Handled = true;
        }
    }
}