using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Windows;
using Microsoft.Win32;

namespace S3StorageClient.Views
{
    public partial class MultipartUploadWindow : Window
    {
        private string _selectedFilePath;
        private string _uploadId;
        private const int ChunkSize = 5 * 1024 * 1024; // 5MB
        private readonly ObservableCollection<PartItem> _parts = new();

        public MultipartUploadWindow()
        {
            InitializeComponent();
            partsList.ItemsSource = _parts;
        }

        private void BtnSelectFile_Click(object sender, RoutedEventArgs e)
        {
            var ofd = new OpenFileDialog();
            if (ofd.ShowDialog() == true)
            {
                _selectedFilePath = ofd.FileName;
                txtFilename.Text = Path.GetFileName(_selectedFilePath);
                btnStart.IsEnabled = true;

                var fileInfo = new FileInfo(_selectedFilePath);
                var numParts = (int)Math.Ceiling((double)fileInfo.Length / ChunkSize);

                _parts.Clear();
                for (int i = 1; i <= numParts; i++)
                {
                    var partSize = i == numParts
                        ? fileInfo.Length - (i - 1) * ChunkSize
                        : ChunkSize;
                    _parts.Add(new PartItem
                    {
                        PartNumber = i.ToString(),
                        PartSize = FormatBytes(partSize),
                        Status = "Pending"
                    });
                }

                lblProgress.Text = $"File size: {FormatBytes(fileInfo.Length)}, Parts: {numParts}";
            }
        }

        private async void BtnStart_Click(object sender, RoutedEventArgs e)
        {
            if (string.IsNullOrEmpty(_selectedFilePath))
                return;

            btnStart.IsEnabled = false;
            btnAbort.IsEnabled = true;
            btnSelectFile.IsEnabled = false;

            try
            {
                var filename = Path.GetFileName(_selectedFilePath);
                lblStatus.Text = "Initiating upload...";

                var initResponse = await App.ApiService.InitiateMultipartUploadAsync(filename);
                _uploadId = initResponse.UploadId;

                var fileInfo = new FileInfo(_selectedFilePath);
                var numParts = (int)Math.Ceiling((double)fileInfo.Length / ChunkSize);
                var uploadedParts = new List<int>();

                using var stream = File.OpenRead(_selectedFilePath);
                for (int i = 1; i <= numParts; i++)
                {
                    var partSize = i == numParts
                        ? fileInfo.Length - (i - 1) * ChunkSize
                        : ChunkSize;
                    var buffer = new byte[partSize];
                    var bytesRead = 0;
                    while (bytesRead < buffer.Length)
                    {
                        var read = await stream.ReadAsync(buffer, bytesRead, buffer.Length - bytesRead);
                        if (read == 0) break;
                        bytesRead += read;
                    }

                    lblStatus.Text = $"Uploading part {i}/{numParts}...";
                    UpdatePartStatus(i, "Uploading");

                    await App.ApiService.UploadPartAsync(_uploadId, i, buffer);
                    uploadedParts.Add(i);

                    UpdatePartStatus(i, "Completed");
                    progressBar.Value = (int)((double)i / numParts * 100);
                    lblProgress.Text = $"Progress: {i}/{numParts} parts ({progressBar.Value}%)";
                }

                lblStatus.Text = "Completing upload...";
                await App.ApiService.CompleteMultipartUploadAsync(_uploadId, uploadedParts);

                lblStatus.Text = "Upload completed successfully!";
                MessageBox.Show("File uploaded successfully!", "Success",
                    MessageBoxButton.OK, MessageBoxImage.Information);
                this.DialogResult = true;
            }
            catch (Exception ex)
            {
                lblStatus.Text = $"Error: {ex.Message}";
                MessageBox.Show($"Upload error: {ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
            finally
            {
                btnStart.IsEnabled = true;
                btnAbort.IsEnabled = false;
                btnSelectFile.IsEnabled = true;
            }
        }

        private async void BtnAbort_Click(object sender, RoutedEventArgs e)
        {
            if (string.IsNullOrEmpty(_uploadId))
                return;

            try
            {
                await App.ApiService.AbortMultipartUploadAsync(_uploadId);
                lblStatus.Text = "Upload aborted";
                MessageBox.Show("Upload aborted", "Info",
                    MessageBoxButton.OK, MessageBoxImage.Information);
                this.DialogResult = false;
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error aborting upload: {ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void UpdatePartStatus(int partNumber, string status)
        {
            foreach (var part in _parts)
            {
                if (part.PartNumber == partNumber.ToString())
                {
                    part.Status = status;
                    break;
                }
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

        private class PartItem
        {
            public string PartNumber { get; set; } = "";
            public string PartSize { get; set; } = "";
            public string Status { get; set; } = "Pending";
        }
    }
}