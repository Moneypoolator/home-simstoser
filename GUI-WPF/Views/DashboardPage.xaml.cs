using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using Microsoft.Win32;
using S3StorageClient.Models;

namespace S3StorageClient.Views
{
    public partial class DashboardPage : UserControl
    {
        public DashboardPage()
        {
            InitializeComponent();
        }

        public async void LoadData()
        {
            try
            {
                lblStatus.Text = "Loading...";
                var data = await App.ApiService.ListFilesAsync();
                var files = data.Files ?? new List<FileMetadata>();

                // Update stats
                statsPanel.Children.Clear();
                var totalFiles = CreateStatCard("Total Files", files.Count.ToString(), Color.FromRgb(37, 99, 235));
                var totalSize = CreateStatCard("Total Size", FormatBytes(files.Sum(f => f.Size)), Color.FromRgb(16, 185, 129));
                var recentCount = CreateStatCard("Recent (7 days)", files.Count(f =>
                {
                    if (DateTime.TryParse(f.LastModified, out var dt))
                        return (DateTime.UtcNow - dt).TotalDays <= 7;
                    return false;
                }).ToString(), Color.FromRgb(59, 130, 246));

                statsPanel.Children.Add(totalFiles);
                statsPanel.Children.Add(totalSize);
                statsPanel.Children.Add(recentCount);

                // Update recent files
                recentFilesList.Items.Clear();
                foreach (var file in files.OrderByDescending(f => f.LastModified).Take(20))
                {
                    recentFilesList.Items.Add(new
                    {
                        Name = file.Name,
                        Size = FormatBytes(file.Size),
                        LastModified = FormatDate(file.LastModified)
                    });
                }

                lblStatus.Text = $"Loaded {files.Count} files";
            }
            catch (Exception ex)
            {
                lblStatus.Text = $"Error: {ex.Message}";
            }
        }

        private Border CreateStatCard(string title, string value, Color accentColor)
        {
            var border = new Border
            {
                Width = 200,
                Height = 90,
                Background = Brushes.White,
                CornerRadius = new CornerRadius(6),
                Margin = new Thickness(0, 0, 15, 0)
            };

            var stack = new StackPanel { Margin = new Thickness(15, 15, 15, 15) };
            stack.Children.Add(new TextBlock
            {
                Text = title,
                FontSize = 11,
                Foreground = new SolidColorBrush(Color.FromRgb(107, 114, 128))
            });
            stack.Children.Add(new TextBlock
            {
                Text = value,
                FontSize = 22,
                FontWeight = FontWeights.Bold,
                Foreground = new SolidColorBrush(accentColor)
            });

            border.Child = stack;
            return border;
        }

        private async void BtnUpload_Click(object sender, RoutedEventArgs e)
        {
            var ofd = new OpenFileDialog { Multiselect = true };
            if (ofd.ShowDialog() == true)
            {
                foreach (var filePath in ofd.FileNames)
                {
                    try
                    {
                        var filename = System.IO.Path.GetFileName(filePath);
                        var data = await System.IO.File.ReadAllBytesAsync(filePath);
                        await App.ApiService.UploadFileAsync(filename, data);
                    }
                    catch (Exception ex)
                    {
                        MessageBox.Show($"Error uploading {System.IO.Path.GetFileName(filePath)}: {ex.Message}",
                            "Upload Error", MessageBoxButton.OK, MessageBoxImage.Error);
                    }
                }
                LoadData();
            }
        }

        private static string FormatBytes(long bytes)
        {
            string[] sizes = { "B", "KB", "MB", "GB", "TB" };
            double len = bytes;
            int order = 0;
            while (len >= 1024 && order < sizes.Length - 1)
            {
                order++;
                len /= 1024;
            }
            return $"{len:0.##} {sizes[order]}";
        }

        private static string FormatDate(string dateString)
        {
            if (DateTime.TryParse(dateString, out var dt))
                return dt.ToString("MMM dd, yyyy HH:mm");
            return dateString;
        }
    }
}