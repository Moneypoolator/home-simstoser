using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;
using S3StorageClient.Models;

namespace S3StorageClient.Views
{
    public partial class UsersPage : UserControl
    {
        public UsersPage()
        {
            InitializeComponent();
        }

        public async void LoadUsers()
        {
            try
            {
                lblStatus.Text = "Loading users...";
                var data = await App.ApiService.ListUsersAsync();
                var users = data.Users ?? new List<User>();

                usersList.Items.Clear();
                foreach (var user in users)
                {
                    usersList.Items.Add(new UserDisplayItem
                    {
                        Username = user.Username,
                        Email = user.Email,
                        Role = GetRoleDisplayName(user.Role),
                        Status = user.IsActive ? "Active" : "Inactive",
                        LastLogin = FormatDate(user.LastLogin),
                        CreatedAt = FormatDate(user.CreatedAt),
                        Tag = user
                    });
                }

                lblStatus.Text = $"Loaded {users.Count} users";
            }
            catch (Exception ex)
            {
                lblStatus.Text = $"Error: {ex.Message}";
                MessageBox.Show($"Error loading users: {ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void BtnCreate_Click(object sender, RoutedEventArgs e)
        {
            var window = new UserFormWindow();
            window.Owner = Window.GetWindow(this);
            if (window.ShowDialog() == true)
            {
                LoadUsers();
            }
        }

        private async void BtnActivate_Click(object sender, RoutedEventArgs e)
        {
            if (usersList.SelectedItem == null) return;
            var user = (User)((UserDisplayItem)usersList.SelectedItem).Tag;

            try
            {
                await App.ApiService.ActivateUserAsync(user.UserId);
                LoadUsers();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error activating user: {ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private async void BtnDeactivate_Click(object sender, RoutedEventArgs e)
        {
            if (usersList.SelectedItem == null) return;
            var user = (User)((UserDisplayItem)usersList.SelectedItem).Tag;

            try
            {
                await App.ApiService.DeactivateUserAsync(user.UserId);
                LoadUsers();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error deactivating user: {ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private async void BtnDelete_Click(object sender, RoutedEventArgs e)
        {
            if (usersList.SelectedItem == null) return;
            var user = (User)((UserDisplayItem)usersList.SelectedItem).Tag;

            var result = MessageBox.Show($"Are you sure you want to delete user '{user.Username}'?",
                "Confirm Delete", MessageBoxButton.YesNo, MessageBoxImage.Question);

            if (result == MessageBoxResult.Yes)
            {
                try
                {
                    await App.ApiService.DeleteUserAsync(user.UserId);
                    LoadUsers();
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Error deleting user: {ex.Message}", "Error",
                        MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
        }

        private void BtnRefresh_Click(object sender, RoutedEventArgs e) => LoadUsers();

        private static string GetRoleDisplayName(string role)
        {
            return role switch
            {
                "ADMIN" => "Administrator",
                "MANAGER" => "Manager",
                "CONTRIBUTOR" => "Contributor",
                "VIEWER" => "Viewer",
                "GUEST" => "Guest",
                _ => role
            };
        }

        private static string FormatDate(string? dateString)
        {
            if (string.IsNullOrEmpty(dateString)) return "Never";
            if (DateTime.TryParse(dateString, out var dt))
                return dt.ToString("MMM dd, yyyy HH:mm");
            return dateString;
        }

        private class UserDisplayItem
        {
            public string Username { get; set; } = "";
            public string Email { get; set; } = "";
            public string Role { get; set; } = "";
            public string Status { get; set; } = "";
            public string LastLogin { get; set; } = "";
            public string CreatedAt { get; set; } = "";
            public object Tag { get; set; } = "";
        }
    }
}