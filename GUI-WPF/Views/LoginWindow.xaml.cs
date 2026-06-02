using System;
using System.Windows;
using System.Windows.Input;
using S3StorageClient.Properties;
using S3StorageClient.Services;

namespace S3StorageClient.Views
{
    public partial class LoginWindow : Window
    {
        public LoginWindow()
        {
            InitializeComponent();
            txtServerUrl.Text = App.ApiService.GetBaseUrl();
        }

        private void TxtServerUrl_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter) BtnLogin_Click(sender, e);
        }

        private void TxtUsername_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter) BtnLogin_Click(sender, e);
        }

        private void TxtPassword_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter) BtnLogin_Click(sender, e);
        }

        private async void BtnLogin_Click(object sender, RoutedEventArgs e)
        {
            var serverUrl = txtServerUrl.Text.Trim();
            var username = txtUsername.Text.Trim();
            var password = txtPassword.Password;

            if (string.IsNullOrEmpty(serverUrl))
            {
                ShowError("Please enter the server URL");
                return;
            }

            if (string.IsNullOrEmpty(username) || string.IsNullOrEmpty(password))
            {
                ShowError("Please fill in all fields");
                return;
            }

            btnLogin.IsEnabled = false;
            btnLogin.Content = "Signing in...";
            lblError.Visibility = Visibility.Collapsed;

            try
            {
                App.ApiService.SetBaseUrl(serverUrl);
                Settings.Default.ServerUrl = serverUrl;
                Settings.Default.Save();

                await App.AuthService.LoginAsync(username, password);

                var mainWindow = new MainWindow();
                mainWindow.Show();
                this.Close();
            }
            catch (UnauthorizedAccessException)
            {
                ShowError("Invalid credentials. Please check your username and password.");
            }
            catch (ApiException ex)
            {
                ShowError($"Server error: {ex.Message}");
            }
            catch (Exception ex)
            {
                ShowError($"Connection error: {ex.Message}");
            }
            finally
            {
                btnLogin.IsEnabled = true;
                btnLogin.Content = "Sign In";
            }
        }

        private void ShowError(string message)
        {
            lblError.Text = message;
            lblError.Visibility = Visibility.Visible;
        }
    }
}