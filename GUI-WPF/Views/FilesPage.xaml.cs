using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using Microsoft.Win32;
using S3StorageClient.Models;

namespace S3StorageClient.Views
{
    public partial class FilesPage : UserControl
    {
        private List<FileMetadata> _allFiles = new();

        public FilesPage()
        {
            InitializeComponent();
        }

        public async void LoadFiles()
        {
            try
            {
                lblStatus.Text = "Loading files...";
                var data = await App.ApiService.ListFilesAsync();
                _allFiles = data.Files ?? new List<FileMetadata>();
                PopulateList(_allFiles);
                lblStatus.Text = $"Loaded {_allFiles.Count} files";
            }
            catch (Exception ex)
            {
                lblStatus.Text = $"Error: {ex.Message}";
                MessageBox.Show($"Error loading files: {ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void PopulateList(List<FileMetadata> files)
        {
            filesList.Items.Clear();
            foreach (var file in files)
            {
                filesList.Items.Add(new FileDisplayItem
                {
                    Name = file.Name,
                    Size = FormatBytes(file.Size),
                    LastModified = FormatDate(file.LastModified),
                    Etag = file.Etag,
                    Tag = file
                });
            }
        }

        private void TxtSearch_TextChanged(object sender, TextChangedEventArgs e)
        {
            var search = txtSearch.Text.ToLower();
            if (string.IsNullOrEmpty(search))
            {
                PopulateList(_allFiles);
            }
            else
            {
                var filtered = _allFiles.Where(f =>
                    f.Name.ToLower().Contains(search)).ToList();
                PopulateList(filtered);
            }
        }

        private async void BtnUpload_Click(object sender, RoutedEventArgs e)
        {
            var ofd = new OpenFileDialog { Multiselect = true };
            if (ofd.ShowDialog() == true)
            {
                lblStatus.Text = "Uploading files...";
                foreach (var filePath in ofd.FileNames)
                {
                    try
                    {
                        var filename = System.IO.Path.GetFileName(filePath);
                        byte[] data;
                        using (var fs = new System.IO.FileStream(filePath, System.IO.FileMode.Open, System.IO.FileAccess.Read, System.IO.FileShare.Read, 4096, true))
                        {
                            data = new byte[fs.Length];
                            await fs.ReadAsync(data, 0, data.Length);
                        }
                        await App.ApiService.UploadFileAsync(filename, data);
                    }
                    catch (Exception ex)
                    {
                        MessageBox.Show($"Error uploading {System.IO.Path.GetFileName(filePath)}: {ex.Message}",
                            "Upload Error", MessageBoxButton.OK, MessageBoxImage.Error);
                    }
                }
                LoadFiles();
            }
        }

        private async void BtnDownload_Click(object sender, RoutedEventArgs e)
        {
            if (filesList.SelectedItem == null)
            {
                MessageBox.Show("Please select a file to download", "Info",
                    MessageBoxButton.OK, MessageBoxImage.Information);
                return;
            }

            var selectedFile = (FileDisplayItem)filesList.SelectedItem;
            var fileMeta = (FileMetadata)selectedFile.Tag;

            var sfd = new SaveFileDialog { FileName = fileMeta.Name };
            if (sfd.ShowDialog() == true)
            {
                try
                {
                    lblStatus.Text = $"Downloading {fileMeta.Name}...";
                    var data = await App.ApiService.DownloadFileAsync(fileMeta.Name);
                    using (var fs = new System.IO.FileStream(sfd.FileName, System.IO.FileMode.Create, System.IO.FileAccess.Write, System.IO.FileShare.None, 4096, true))
                    {
                        await fs.WriteAsync(data, 0, data.Length);
                    }
                    lblStatus.Text = $"Downloaded {fileMeta.Name}";
                    MessageBox.Show("File downloaded successfully", "Success",
                        MessageBoxButton.OK, MessageBoxImage.Information);
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Error downloading file: {ex.Message}", "Error",
                        MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
        }

        private async void BtnDelete_Click(object sender, RoutedEventArgs e)
        {
            if (filesList.SelectedItem == null)
            {
                MessageBox.Show("Please select a file to delete", "Info",
                    MessageBoxButton.OK, MessageBoxImage.Information);
                return;
            }

            var selectedFile = (FileDisplayItem)filesList.SelectedItem;
            var fileMeta = (FileMetadata)selectedFile.Tag;

            var result = MessageBox.Show($"Are you sure you want to delete '{fileMeta.Name}'?",
                "Confirm Delete", MessageBoxButton.YesNo, MessageBoxImage.Question);

            if (result == MessageBoxResult.Yes)
            {
                try
                {
                    lblStatus.Text = $"Deleting {fileMeta.Name}...";
                    await App.ApiService.DeleteFileAsync(fileMeta.Name);
                    LoadFiles();
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Error deleting file: {ex.Message}", "Error",
                        MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
        }

        private void BtnMultipart_Click(object sender, RoutedEventArgs e)
        {
            var window = new MultipartUploadWindow();
            window.Owner = Window.GetWindow(this);
            if (window.ShowDialog() == true)
            {
                LoadFiles();
            }
        }

        private void BtnRefresh_Click(object sender, RoutedEventArgs e) => LoadFiles();

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

        private class FileDisplayItem
        {
            public string Name { get; set; } = "";
            public string Size { get; set; } = "";
            public string LastModified { get; set; } = "";
            public string Etag { get; set; } = "";
            public object Tag { get; set; } = "";
        }
    }
}