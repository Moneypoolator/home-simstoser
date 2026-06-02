using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows;
using S3StorageClient.Models;

namespace S3StorageClient.Views
{
    public partial class CreateKeyWindow : Window
    {
        public CreateKeyWindow()
        {
            InitializeComponent();
            Loaded += CreateKeyWindow_Loaded;
        }

        private async void CreateKeyWindow_Loaded(object sender, RoutedEventArgs e)
        {
            await LoadUsersAsync();
        }

        private async System.Threading.Tasks.Task LoadUsersAsync()
        {
            try
            {
                var data = await App.ApiService.ListUsersAsync();
                var activeUsers = data.Users?.Where(u => u.IsActive).ToList() ?? new List<User>();
                cmbUser.ItemsSource = activeUsers;
                if (cmbUser.Items.Count > 0)
                    cmbUser.SelectedIndex = 0;
            }
            catch { }
        }

        private async void BtnSave_Click(object sender, RoutedEventArgs e)
        {
            if (cmbUser.SelectedItem == null)
            {
                MessageBox.Show("Please select a user", "Validation Error",
                    MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            try
            {
                var selectedUser = (User)cmbUser.SelectedItem;
                var newKey = await App.ApiService.CreateKeyAsync(selectedUser.Username);

                var secretMsg = $"Access Key created successfully!\n\n" +
                    $"Access Key ID: {newKey.AccessKeyId}\n" +
                    $"Secret Key: {newKey.SecretAccessKey}\n\n" +
                    "IMPORTANT: Copy the secret key now - it will not be shown again!";

                MessageBox.Show(secretMsg, "Key Created", MessageBoxButton.OK, MessageBoxImage.Information);

                this.DialogResult = true;
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error creating key: {ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void BtnCancel_Click(object sender, RoutedEventArgs e)
        {
            this.DialogResult = false;
        }
    }
}