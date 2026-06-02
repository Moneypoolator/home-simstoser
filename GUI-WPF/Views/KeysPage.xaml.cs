using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;
using S3StorageClient.Models;

namespace S3StorageClient.Views
{
    public partial class KeysPage : UserControl
    {
        public KeysPage()
        {
            InitializeComponent();
        }

        public async void LoadKeys()
        {
            try
            {
                lblStatus.Text = "Loading keys...";
                var data = await App.ApiService.ListKeysAsync();
                var keys = data.Keys ?? new List<AccessKey>();

                keysList.Items.Clear();
                foreach (var key in keys)
                {
                    keysList.Items.Add(new KeyDisplayItem
                    {
                        AccessKeyId = key.AccessKeyId,
                        UserName = key.UserName,
                        Status = key.IsActive ? "Active" : "Inactive",
                        CreatedAt = FormatDate(key.CreatedAt),
                        Tag = key
                    });
                }

                lblStatus.Text = $"Loaded {keys.Count} keys";
            }
            catch (Exception ex)
            {
                lblStatus.Text = $"Error: {ex.Message}";
                MessageBox.Show($"Error loading keys: {ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void BtnCreate_Click(object sender, RoutedEventArgs e)
        {
            var window = new CreateKeyWindow();
            window.Owner = Window.GetWindow(this);
            if (window.ShowDialog() == true)
            {
                LoadKeys();
            }
        }

        private async void BtnActivate_Click(object sender, RoutedEventArgs e)
        {
            if (keysList.SelectedItem == null) return;
            var key = (AccessKey)((KeyDisplayItem)keysList.SelectedItem).Tag;

            try
            {
                await App.ApiService.ActivateKeyAsync(key.AccessKeyId);
                LoadKeys();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error activating key: {ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private async void BtnDeactivate_Click(object sender, RoutedEventArgs e)
        {
            if (keysList.SelectedItem == null) return;
            var key = (AccessKey)((KeyDisplayItem)keysList.SelectedItem).Tag;

            try
            {
                await App.ApiService.DeactivateKeyAsync(key.AccessKeyId);
                LoadKeys();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error deactivating key: {ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private async void BtnDelete_Click(object sender, RoutedEventArgs e)
        {
            if (keysList.SelectedItem == null) return;
            var key = (AccessKey)((KeyDisplayItem)keysList.SelectedItem).Tag;

            var result = MessageBox.Show($"Are you sure you want to delete key '{key.AccessKeyId}'?",
                "Confirm Delete", MessageBoxButton.YesNo, MessageBoxImage.Question);

            if (result == MessageBoxResult.Yes)
            {
                try
                {
                    await App.ApiService.DeleteKeyAsync(key.AccessKeyId);
                    LoadKeys();
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Error deleting key: {ex.Message}", "Error",
                        MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
        }

        private void BtnRefresh_Click(object sender, RoutedEventArgs e) => LoadKeys();

        private static string FormatDate(string dateString)
        {
            if (DateTime.TryParse(dateString, out var dt))
                return dt.ToString("MMM dd, yyyy HH:mm");
            return dateString;
        }

        private class KeyDisplayItem
        {
            public string AccessKeyId { get; set; } = "";
            public string UserName { get; set; } = "";
            public string Status { get; set; } = "";
            public string CreatedAt { get; set; } = "";
            public object Tag { get; set; } = "";
        }
    }
}