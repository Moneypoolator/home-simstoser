using System;
using System.Windows;
using System.Windows.Controls;
using S3StorageClient.Models;

namespace S3StorageClient.Views
{
    public partial class UserFormWindow : Window
    {
        public UserFormWindow()
        {
            InitializeComponent();
            cmbRole.SelectedIndex = 3; // Default to VIEWER
        }

        private async void BtnSave_Click(object sender, RoutedEventArgs e)
        {
            if (string.IsNullOrWhiteSpace(txtUsername.Text))
            {
                MessageBox.Show("Username is required", "Validation Error",
                    MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            var role = (cmbRole.SelectedItem as ComboBoxItem)?.Content?.ToString() ?? "VIEWER";

            try
            {
                await App.ApiService.CreateUserAsync(
                    txtUsername.Text.Trim(),
                    txtEmail.Text.Trim(),
                    role
                );

                MessageBox.Show("User created successfully!", "Success",
                    MessageBoxButton.OK, MessageBoxImage.Information);
                this.DialogResult = true;
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error creating user: {ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void BtnCancel_Click(object sender, RoutedEventArgs e)
        {
            this.DialogResult = false;
        }
    }
}