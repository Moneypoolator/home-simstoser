using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;
using S3StorageClient.Models;

namespace S3StorageClient.Views
{
    public partial class PoliciesPage : UserControl
    {
        public PoliciesPage()
        {
            InitializeComponent();
        }

        public async void LoadPolicies()
        {
            try
            {
                lblStatus.Text = "Loading policies...";
                var data = await App.ApiService.ListPoliciesAsync();
                var policies = data.Policies ?? new List<AccessPolicy>();

                policiesList.Items.Clear();
                foreach (var policy in policies)
                {
                    policiesList.Items.Add(new PolicyDisplayItem
                    {
                        Name = policy.Name,
                        Description = policy.Description,
                        PermissionsText = FormatPermissions(policy.Permissions),
                        CreatedAt = FormatDate(policy.CreatedAt),
                        Tag = policy
                    });
                }

                lblStatus.Text = $"Loaded {policies.Count} policies";
            }
            catch (Exception ex)
            {
                lblStatus.Text = $"Error: {ex.Message}";
                MessageBox.Show($"Error loading policies: {ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void BtnCreate_Click(object sender, RoutedEventArgs e)
        {
            var window = new PolicyFormWindow();
            window.Owner = Window.GetWindow(this);
            if (window.ShowDialog() == true)
            {
                LoadPolicies();
            }
        }

        private void BtnEdit_Click(object sender, RoutedEventArgs e)
        {
            if (policiesList.SelectedItem == null) return;
            var policy = (AccessPolicy)((PolicyDisplayItem)policiesList.SelectedItem).Tag;

            var window = new PolicyFormWindow(policy);
            window.Owner = Window.GetWindow(this);
            if (window.ShowDialog() == true)
            {
                LoadPolicies();
            }
        }

        private async void BtnDelete_Click(object sender, RoutedEventArgs e)
        {
            if (policiesList.SelectedItem == null) return;
            var policy = (AccessPolicy)((PolicyDisplayItem)policiesList.SelectedItem).Tag;

            var result = MessageBox.Show($"Are you sure you want to delete policy '{policy.Name}'?",
                "Confirm Delete", MessageBoxButton.YesNo, MessageBoxImage.Question);

            if (result == MessageBoxResult.Yes)
            {
                try
                {
                    await App.ApiService.DeletePolicyAsync(policy.PolicyId);
                    LoadPolicies();
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Error deleting policy: {ex.Message}", "Error",
                        MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
        }

        private void BtnRefresh_Click(object sender, RoutedEventArgs e) => LoadPolicies();

        private static string FormatPermissions(List<Permission> permissions)
        {
            if (permissions == null || permissions.Count == 0)
                return "None";

            var parts = new List<string>();
            foreach (var perm in permissions)
            {
                var allowDeny = perm.Allow ? "✓" : "✗";
                parts.Add($"{perm.Type} {allowDeny} ({perm.ResourcePattern})");
            }
            return string.Join(", ", parts);
        }

        private static string FormatDate(string dateString)
        {
            if (DateTime.TryParse(dateString, out var dt))
                return dt.ToString("MMM dd, yyyy HH:mm");
            return dateString;
        }

        private class PolicyDisplayItem
        {
            public string Name { get; set; } = "";
            public string Description { get; set; } = "";
            public string PermissionsText { get; set; } = "";
            public string CreatedAt { get; set; } = "";
            public object Tag { get; set; } = "";
        }
    }
}