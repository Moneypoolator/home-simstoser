using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;
using S3StorageClient.Models;

namespace S3StorageClient.Views
{
    public partial class PolicyFormWindow : Window
    {
        private readonly AccessPolicy _existingPolicy;
        private readonly List<PermissionRowControl> _permissionRows = new();

        public PolicyFormWindow(AccessPolicy existingPolicy = null)
        {
            _existingPolicy = existingPolicy;
            InitializeComponent();

            if (existingPolicy != null)
            {
                Title = "Edit Policy";
                btnSave.Content = "Save Changes";
                txtName.Text = existingPolicy.Name;
                txtDescription.Text = existingPolicy.Description;

                foreach (var perm in existingPolicy.Permissions)
                {
                    AddPermissionRow(perm.Type, perm.ResourcePattern, perm.Allow);
                }
            }
            else
            {
                // Add one empty permission row by default
                AddPermissionRow("READ", "*", true);
            }
        }

        private void AddPermissionRow(string type, string pattern, bool allow)
        {
            var row = new PermissionRowControl(type, pattern, allow);
            row.RemoveRequested += (s, e) =>
            {
                _permissionRows.Remove(row);
                permissionsStack.Children.Remove(row);
            };
            _permissionRows.Add(row);
            permissionsStack.Children.Add(row);
        }

        private void BtnAddPermission_Click(object sender, RoutedEventArgs e)
        {
            AddPermissionRow("READ", "*", true);
        }

        private async void BtnSave_Click(object sender, RoutedEventArgs e)
        {
            if (string.IsNullOrWhiteSpace(txtName.Text))
            {
                MessageBox.Show("Policy name is required", "Validation Error",
                    MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            var permissions = new List<Permission>();
            foreach (var row in _permissionRows)
            {
                permissions.Add(new Permission
                {
                    Type = row.SelectedType,
                    ResourcePattern = row.ResourcePattern,
                    Allow = row.Allow
                });
            }

            if (permissions.Count == 0)
            {
                MessageBox.Show("At least one permission is required", "Validation Error",
                    MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            try
            {
                if (_existingPolicy != null)
                {
                    await App.ApiService.UpdatePolicyAsync(
                        _existingPolicy.PolicyId,
                        txtName.Text.Trim(),
                        txtDescription.Text.Trim(),
                        permissions
                    );
                }
                else
                {
                    await App.ApiService.CreatePolicyAsync(
                        txtName.Text.Trim(),
                        txtDescription.Text.Trim(),
                        permissions
                    );
                }
                this.DialogResult = true;
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error saving policy: {ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void BtnCancel_Click(object sender, RoutedEventArgs e)
        {
            this.DialogResult = false;
        }

        /// <summary>
        /// Represents a single permission row with type ComboBox, pattern TextBox, allow CheckBox, and remove Button.
        /// </summary>
        private class PermissionRowControl : Border
        {
            private readonly ComboBox cmbType;
            private readonly TextBox txtPattern;
            private readonly CheckBox chkAllow;

            public string SelectedType => cmbType.SelectedItem?.ToString() ?? "READ";
            public string ResourcePattern => txtPattern.Text.Trim();
            public bool Allow => chkAllow.IsChecked ?? true;

            public event EventHandler RemoveRequested;

            public PermissionRowControl(string type, string pattern, bool allow)
            {
                Background = System.Windows.Media.Brushes.White;
                BorderBrush = System.Windows.Media.Brushes.Transparent;
                Margin = new Thickness(0, 0, 0, 5);
                Padding = new Thickness(5);
                CornerRadius = new CornerRadius(4);

                var grid = new Grid();
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(110) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(200) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(80) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(30) });

                // Type ComboBox
                cmbType = new ComboBox
                {
                    Height = 28,
                    FontSize = 12,
                    Margin = new Thickness(0, 0, 5, 0)
                };
                cmbType.Items.Add("READ");
                cmbType.Items.Add("WRITE");
                cmbType.Items.Add("DELETE");
                cmbType.Items.Add("LIST");
                cmbType.Items.Add("MANAGE_ACL");
                cmbType.SelectedItem = type;
                Grid.SetColumn(cmbType, 0);
                grid.Children.Add(cmbType);

                // Resource pattern TextBox
                txtPattern = new TextBox
                {
                    Height = 28,
                    FontSize = 12,
                    Text = pattern,
                    VerticalContentAlignment = VerticalAlignment.Center,
                    Margin = new Thickness(0, 0, 5, 0)
                };
                Grid.SetColumn(txtPattern, 1);
                grid.Children.Add(txtPattern);

                // Allow CheckBox
                chkAllow = new CheckBox
                {
                    Content = "Allow",
                    IsChecked = allow,
                    VerticalAlignment = VerticalAlignment.Center,
                    FontSize = 12,
                    Margin = new Thickness(5, 0, 0, 0)
                };
                Grid.SetColumn(chkAllow, 2);
                grid.Children.Add(chkAllow);

                // Remove button
                var btnRemove = new Button
                {
                    Content = "✕",
                    Width = 25,
                    Height = 25,
                    FontSize = 11,
                    Foreground = System.Windows.Media.Brushes.Red,
                    Background = System.Windows.Media.Brushes.Transparent,
                    BorderBrush = System.Windows.Media.Brushes.Transparent,
                    Cursor = System.Windows.Input.Cursors.Hand,
                    ToolTip = "Remove permission"
                };
                btnRemove.Click += (s, e) => RemoveRequested?.Invoke(this, EventArgs.Empty);
                Grid.SetColumn(btnRemove, 3);
                grid.Children.Add(btnRemove);

                Child = grid;
            }
        }
    }
}