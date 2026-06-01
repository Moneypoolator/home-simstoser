using System;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;
using S3StorageClient.Models;

namespace S3StorageClient.Forms
{
    public class FilesControl : UserControl
    {
        private ListView filesList;
        private Button btnUpload;
        private Button btnDownload;
        private Button btnDelete;
        private Button btnRefresh;
        private Button btnMultipart;
        private Label lblStatus;
        private TextBox txtSearch;

        public FilesControl()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            this.BackColor = Color.FromArgb(249, 250, 251);
            this.Padding = new Padding(10);

            // Toolbar
            var toolbar = new Panel
            {
                Dock = DockStyle.Top,
                Height = 50,
                BackColor = Color.White
            };

            btnUpload = CreateToolbarButton("Upload", 10);
            btnUpload.Click += BtnUpload_Click;

            btnDownload = CreateToolbarButton("Download", 100);
            btnDownload.Click += BtnDownload_Click;

            btnDelete = CreateToolbarButton("Delete", 190);
            btnDelete.Click += BtnDelete_Click;

            btnMultipart = CreateToolbarButton("Multipart Upload", 280);
            btnMultipart.Click += BtnMultipart_Click;

            btnRefresh = CreateToolbarButton("Refresh", 420);
            btnRefresh.Click += (s, e) => LoadFiles();

            txtSearch = new TextBox
            {
                Font = new Font("Segoe UI", 10),
                Location = new Point(toolbar.Width - 220, 10),
                Size = new Size(200, 30),
                Anchor = AnchorStyles.Top | AnchorStyles.Right,
                BorderStyle = BorderStyle.FixedSingle
            };
            txtSearch.TextChanged += (s, e) => FilterFiles();

            toolbar.Controls.AddRange(new Control[] { btnUpload, btnDownload, btnDelete, btnMultipart, btnRefresh, txtSearch });

            // Files list
            filesList = new ListView
            {
                View = View.Details,
                FullRowSelect = true,
                GridLines = true,
                Dock = DockStyle.Fill,
                MultiSelect = true
            };
            filesList.Columns.Add("Name", 350);
            filesList.Columns.Add("Size", 100);
            filesList.Columns.Add("Last Modified", 180);
            filesList.Columns.Add("ETag", 200);

            // Status bar
            lblStatus = new Label
            {
                Dock = DockStyle.Bottom,
                Height = 25,
                TextAlign = ContentAlignment.MiddleLeft,
                ForeColor = Color.FromArgb(107, 114, 128),
                Font = new Font("Segoe UI", 9, FontStyle.Italic),
                BackColor = Color.White,
                Padding = new Padding(5, 0, 0, 0)
            };

            this.Controls.Add(filesList);
            this.Controls.Add(toolbar);
            this.Controls.Add(lblStatus);
        }

        private Button CreateToolbarButton(string text, int x)
        {
            return new Button
            {
                Text = text,
                Font = new Font("Segoe UI", 9, FontStyle.Regular),
                BackColor = Color.FromArgb(37, 99, 235),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                FlatAppearance = { BorderSize = 0 },
                Size = new Size(80, 30),
                Location = new Point(x, 10),
                Cursor = Cursors.Hand
            };
        }

        private List<FileMetadata> _allFiles = new();

        public async void LoadFiles()
        {
            try
            {
                lblStatus.Text = "Loading files...";
                var data = await Program.ApiService.ListFilesAsync();
                _allFiles = data.Files ?? new List<FileMetadata>();
                PopulateList(_allFiles);
                lblStatus.Text = $"Loaded {_allFiles.Count} files";
            }
            catch (Exception ex)
            {
                lblStatus.Text = $"Error: {ex.Message}";
                MessageBox.Show($"Error loading files: {ex.Message}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void PopulateList(List<FileMetadata> files)
        {
            filesList.Items.Clear();
            foreach (var file in files)
            {
                var item = new ListViewItem(file.Name);
                item.SubItems.Add(FormatBytes(file.Size));
                item.SubItems.Add(FormatDate(file.LastModified));
                item.SubItems.Add(file.Etag);
                item.Tag = file;
                filesList.Items.Add(item);
            }
        }

        private void FilterFiles()
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

        private async void BtnUpload_Click(object? sender, EventArgs e)
        {
            using var ofd = new OpenFileDialog { Multiselect = true };
            if (ofd.ShowDialog() == DialogResult.OK)
            {
                lblStatus.Text = "Uploading files...";
                foreach (var filePath in ofd.FileNames)
                {
                    try
                    {
                        var filename = Path.GetFileName(filePath);
                        var data = await File.ReadAllBytesAsync(filePath);
                        await Program.ApiService.UploadFileAsync(filename, data);
                    }
                    catch (Exception ex)
                    {
                        MessageBox.Show($"Error uploading {Path.GetFileName(filePath)}: {ex.Message}",
                            "Upload Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                }
                LoadFiles();
            }
        }

        private async void BtnDownload_Click(object? sender, EventArgs e)
        {
            if (filesList.SelectedItems.Count == 0)
            {
                MessageBox.Show("Please select a file to download", "Info",
                    MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }

            var selectedFile = (FileMetadata)filesList.SelectedItems[0].Tag;
            using var sfd = new SaveFileDialog { FileName = selectedFile.Name };
            if (sfd.ShowDialog() == DialogResult.OK)
            {
                try
                {
                    lblStatus.Text = $"Downloading {selectedFile.Name}...";
                    var data = await Program.ApiService.DownloadFileAsync(selectedFile.Name);
                    await File.WriteAllBytesAsync(sfd.FileName, data);
                    lblStatus.Text = $"Downloaded {selectedFile.Name}";
                    MessageBox.Show("File downloaded successfully", "Success",
                        MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Error downloading file: {ex.Message}", "Error",
                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }

        private async void BtnDelete_Click(object? sender, EventArgs e)
        {
            if (filesList.SelectedItems.Count == 0)
            {
                MessageBox.Show("Please select a file to delete", "Info",
                    MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }

            var selectedFile = (FileMetadata)filesList.SelectedItems[0].Tag;
            var result = MessageBox.Show($"Are you sure you want to delete '{selectedFile.Name}'?",
                "Confirm Delete", MessageBoxButtons.YesNo, MessageBoxIcon.Question);

            if (result == DialogResult.Yes)
            {
                try
                {
                    lblStatus.Text = $"Deleting {selectedFile.Name}...";
                    await Program.ApiService.DeleteFileAsync(selectedFile.Name);
                    LoadFiles();
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Error deleting file: {ex.Message}", "Error",
                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }

        private void BtnMultipart_Click(object? sender, EventArgs e)
        {
            var form = new MultipartUploadForm();
            form.ShowDialog();
            LoadFiles();
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