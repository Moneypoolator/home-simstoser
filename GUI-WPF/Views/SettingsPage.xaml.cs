using System;
using System.Windows;
using System.Windows.Controls;
using S3StorageClient.Properties;

namespace S3StorageClient.Views
{
    public partial class SettingsPage : UserControl
    {
        public SettingsPage()
        {
            InitializeComponent();
            LoadSettings();
        }

        private void LoadSettings()
        {
            txtServerUrl.Text = Settings.Default.ServerUrl;
            cmbTheme.SelectedIndex = Settings.Default.Theme == "Dark" ? 1 : 0;
            chkNotifications.IsChecked = Settings.Default.Notifications;
            chkAutoRefresh.IsChecked = Settings.Default.AutoRefresh;

            lblUsername.Text = Settings.Default.Username;
            lblUserId.Text = Settings.Default.UserId;
        }

        private void BtnSave_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                Settings.Default.ServerUrl = txtServerUrl.Text.Trim();
                Settings.Default.Theme = (cmbTheme.SelectedItem as ComboBoxItem)?.Content?.ToString() ?? "Light";
                Settings.Default.Notifications = chkNotifications.IsChecked ?? true;
                Settings.Default.AutoRefresh = chkAutoRefresh.IsChecked ?? true;
                Settings.Default.Save();

                // Update API service base URL
                App.ApiService.SetBaseUrl(txtServerUrl.Text.Trim());

                lblStatus.Text = "Settings saved successfully";
                MessageBox.Show("Settings saved successfully", "Settings",
                    MessageBoxButton.OK, MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                lblStatus.Text = $"Error saving settings: {ex.Message}";
                MessageBox.Show($"Error saving settings: {ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void BtnReset_Click(object sender, RoutedEventArgs e)
        {
            var result = MessageBox.Show("Reset all settings to default values?",
                "Confirm Reset", MessageBoxButton.YesNo, MessageBoxImage.Question);

            if (result == MessageBoxResult.Yes)
            {
                Settings.Default.Reset();
                LoadSettings();
                lblStatus.Text = "Settings reset to defaults";
                MessageBox.Show("Settings reset to defaults", "Settings",
                    MessageBoxButton.OK, MessageBoxImage.Information);
            }
        }

        private void BtnLogout_Click(object sender, RoutedEventArgs e)
        {
            var result = MessageBox.Show("Are you sure you want to logout?",
                "Confirm Logout", MessageBoxButton.YesNo, MessageBoxImage.Question);

            if (result == MessageBoxResult.Yes)
            {
                App.AuthService.Logout();

                // Close the main window and show login
                var mainWindow = Window.GetWindow(this);
                if (mainWindow != null)
                {
                    mainWindow.Close();
                }

                var loginWindow = new LoginWindow();
                loginWindow.ShowDialog();

                // If login was successful, restart the application
                Application.Current.Shutdown();
            }
        }
    }
}