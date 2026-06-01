using System;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;
using S3StorageClient.Models;

namespace S3StorageClient.Forms
{
    public class DashboardControl : UserControl
    {
        private FlowLayoutPanel statsPanel;
        private Panel uploadPanel;
        private Panel recentFilesPanel;
        private ListView recentFilesList;
        private Label lblStatus;

        public DashboardControl()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            this.BackColor = Color.FromArgb(249, 250, 251);
            this.Padding = new Padding(10);

            // Stats panel
            statsPanel = new FlowLayoutPanel
            {
                Dock = DockStyle.Top,
                Height = 120,
                Padding = new Padding(0, 0, 0, 20)
            };

            // Upload panel
            uploadPanel = new Panel
            {
                Dock = DockStyle.Top,
                Height = 120,
                BackColor = Color.White,
                Padding = new Padding(20)
            };

            var uploadLabel = new Label
            {
                Text = "Quick Upload",
                Font = new Font("Segoe UI", 12, FontStyle.Bold),
                Location = new Point(20, 15),
                AutoSize = true
            };

            var btnUpload = new Button
            {
                Text = "Upload Files",
                Font = new Font("Segoe UI", 10, FontStyle.Regular),
                BackColor = Color.FromArgb(37, 99, 235),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                FlatAppearance = { BorderSize = 0 },
                Size = new Size(150, 35),
                Location = new Point(20, 50),
                Cursor = Cursors.Hand
            };
            btnUpload.Click += BtnUpload_Click;

            uploadPanel.Controls.AddRange(new Control[] { uploadLabel, btnUpload });

            // Recent files panel
            recentFilesPanel = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = Color.White,
                Padding = new Padding(20)
            };

            var recentLabel = new Label
            {
                Text = "Recent Files",
                Font = new Font("Segoe UI", 12, FontStyle.Bold),
                Location = new Point(20, 15),
                AutoSize = true
            };

            recentFilesList = new ListView
            {
                View = View.Details,
                FullRowSelect = true,
                GridLines = true,
                Location = new Point(20, 45),
                Size = new Size(recentFilesPanel.Width - 40, recentFilesPanel.Height - 65),
                Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right
            };
            recentFilesList.Columns.Add("Name", 300);
            recentFilesList.Columns.Add("Size", 100);
            recentFilesList.Columns.Add("Modified", 180);

            recentFilesPanel.Controls.AddRange(new Control[] { recentLabel, recentFilesList });

            // Status label
            lblStatus = new Label
            {
                Dock = DockStyle.Bottom,
                Height = 30,
                TextAlign = ContentAlignment.MiddleLeft,
                ForeColor = Color.FromArgb(107, 114, 128),
                Font = new Font("Segoe UI", 9, FontStyle.Italic)
            };

            this.Controls.Add(recentFilesPanel);
            this.Controls.Add(uploadPanel);
            this.Controls.Add(statsPanel);
            this.Controls.Add(lblStatus);
        }

        public async void LoadData()
        {
            try
            {
                lblStatus.Text = "Loading...";
                var data = await Program.ApiService.ListFilesAsync();
                var files = data.Files ?? new List<FileMetadata>();

                // Update stats
                statsPanel.Controls.Clear();
                var totalFiles = CreateStatCard("Total Files", files.Count.ToString(), Color.FromArgb(37, 99, 235));
                var totalSize = CreateStatCard("Total Size", FormatBytes(files.Sum(f => f.Size)), Color.FromArgb(16, 185, 129));
                var recentCount = CreateStatCard("Recent (7 days)", files.Count(f =>
                {
                    if (DateTime.TryParse(f.LastModified, out var dt))
                        return (DateTime.UtcNow - dt).TotalDays <= 7;
                    return false;
                }).ToString(), Color.FromArgb(59, 130, 246));

                statsPanel.Controls.AddRange(new[] { totalFiles, totalSize, recentCount });

                // Update recent files
                recentFilesList.Items.Clear();
                foreach (var file in files.OrderByDescending(f => f.LastModified).Take(20))
                {
                    var item = new ListViewItem(file.Name);
                    item.SubItems.Add(FormatBytes(file.Size));
                    item.SubItems.Add(FormatDate(file.LastModified));
                    recentFilesList.Items.Add(item);
                }

                lblStatus.Text = $"Loaded {files.Count} files";
            }
            catch (Exception ex)
            {
                lblStatus.Text = $"Error: {ex.Message}";
            }
        }

        private Panel CreateStatCard(string title, string value, Color accentColor)
        {
            var panel = new Panel
            {
                Size = new Size(200, 90),
                BackColor = Color.White,
                Margin = new Padding(0, 0, 15, 0)
            };

            var titleLabel = new Label
            {
                Text = title,
                Font = new Font("Segoe UI", 9, FontStyle.Regular),
                ForeColor = Color.FromArgb(107, 114, 128),
                Location = new Point(15, 15),
                AutoSize = true
            };

            var valueLabel = new Label
            {
                Text = value,
                Font = new Font("Segoe UI", 20, FontStyle.Bold),
                ForeColor = accentColor,
                Location = new Point(15, 35),
                AutoSize = true
            };

            panel.Controls.AddRange(new[] { titleLabel, valueLabel });
            return panel;
        }

        private async void BtnUpload_Click(object? sender, EventArgs e)
        {
            using var ofd = new OpenFileDialog { Multiselect = true };
            if (ofd.ShowDialog() == DialogResult.OK)
            {
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