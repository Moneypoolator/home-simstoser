using System;
using System.Drawing;
using System.Windows.Forms;

namespace S3StorageClient.Forms
{
    public class MultipartUploadForm : Form
    {
        private TextBox txtFilename;
        private Button btnSelectFile;
        private Button btnStart;
        private Button btnAbort;
        private ProgressBar progressBar;
        private Label lblProgress;
        private ListView partsList;
        private Label lblStatus;
        private string? _selectedFilePath;
        private string? _uploadId;
        private const int ChunkSize = 5 * 1024 * 1024; // 5MB

        public MultipartUploadForm()
        {
            InitializeComponent();
            this.StartPosition = FormStartPosition.CenterParent;
            this.ClientSize = new Size(600, 500);
            this.Text = "Multipart Upload";
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
        }

        private void InitializeComponent()
        {
            this.BackColor = Color.White;
            this.Padding = new Padding(20);

            // File selection
            var lblFile = new Label
            {
                Text = "Select File:",
                Font = new Font("Segoe UI", 10, FontStyle.Bold),
                Location = new Point(20, 20),
                AutoSize = true
            };

            txtFilename = new TextBox
            {
                Font = new Font("Segoe UI", 10),
                Location = new Point(20, 45),
                Size = new Size(400, 30),
                ReadOnly = true,
                BorderStyle = BorderStyle.FixedSingle
            };

            btnSelectFile = new Button
            {
                Text = "Browse...",
                Font = new Font("Segoe UI", 9, FontStyle.Regular),
                Location = new Point(430, 44),
                Size = new Size(100, 30),
                Cursor = Cursors.Hand
            };
            btnSelectFile.Click += BtnSelectFile_Click;

            // Progress
            progressBar = new ProgressBar
            {
                Location = new Point(20, 100),
                Size = new Size(510, 25),
                Style = ProgressBarStyle.Continuous
            };

            lblProgress = new Label
            {
                Text = "Ready",
                Font = new Font("Segoe UI", 9, FontStyle.Regular),
                Location = new Point(20, 130),
                AutoSize = true
            };

            // Parts list
            var lblParts = new Label
            {
                Text = "Upload Parts:",
                Font = new Font("Segoe UI", 10, FontStyle.Bold),
                Location = new Point(20, 160),
                AutoSize = true
            };

            partsList = new ListView
            {
                View = View.Details,
                FullRowSelect = true,
                Location = new Point(20, 185),
                Size = new Size(510, 200)
            };
            partsList.Columns.Add("Part", 80);
            partsList.Columns.Add("Size", 120);
            partsList.Columns.Add("Status", 150);

            // Buttons
            btnStart = new Button
            {
                Text = "Start Upload",
                Font = new Font("Segoe UI", 10, FontStyle.Bold),
                BackColor = Color.FromArgb(37, 99, 235),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                FlatAppearance = { BorderSize = 0 },
                Location = new Point(20, 400),
                Size = new Size(130, 35),
                Cursor = Cursors.Hand,
                Enabled = false
            };
            btnStart.Click += BtnStart_Click;

            btnAbort = new Button
            {
                Text = "Abort",
                Font = new Font("Segoe UI", 10, FontStyle.Regular),
                BackColor = Color.FromArgb(220, 38, 38),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                FlatAppearance = { BorderSize = 0 },
                Location = new Point(160, 400),
                Size = new Size(100, 35),
                Cursor = Cursors.Hand,
                Enabled = false
            };
            btnAbort.Click += BtnAbort_Click;

            lblStatus = new Label
            {
                Font = new Font("Segoe UI", 9, FontStyle.Italic),
                ForeColor = Color.FromArgb(107, 114, 128),
                Location = new Point(20, 445),
                AutoSize = true
            };

            this.Controls.AddRange(new Control[] {
                lblFile, txtFilename, btnSelectFile,
                progressBar, lblProgress,
                lblParts, partsList,
                btnStart, btnAbort, lblStatus
            });
        }

        private void BtnSelectFile_Click(object? sender, EventArgs e)
        {
            using var ofd = new OpenFileDialog();
            if (ofd.ShowDialog() == DialogResult.OK)
            {
                _selectedFilePath = ofd.FileName;
                txtFilename.Text = Path.GetFileName(_selectedFilePath);
                btnStart.Enabled = true;

                var fileInfo = new FileInfo(_selectedFilePath);
                var numParts = (int)Math.Ceiling((double)fileInfo.Length / ChunkSize);

                partsList.Items.Clear();
                for (int i = 1; i <= numParts; i++)
                {
                    var item = new ListViewItem(i.ToString());
                    var partSize = i == numParts
                        ? fileInfo.Length - (i - 1) * ChunkSize
                        : ChunkSize;
                    item.SubItems.Add(FormatBytes(partSize));
                    item.SubItems.Add("Pending");
                    partsList.Items.Add(item);
                }

                lblProgress.Text = $"File size: {FormatBytes(fileInfo.Length)}, Parts: {numParts}";
            }
        }

        private async void BtnStart_Click(object? sender, EventArgs e)
        {
            if (string.IsNullOrEmpty(_selectedFilePath))
                return;

            btnStart.Enabled = false;
            btnAbort.Enabled = true;
            btnSelectFile.Enabled = false;

            try
            {
                var filename = Path.GetFileName(_selectedFilePath);
                lblStatus.Text = "Initiating upload...";

                var initResponse = await Program.ApiService.InitiateMultipartUploadAsync(filename);
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
                    await stream.ReadAsync(buffer);

                    lblStatus.Text = $"Uploading part {i}/{numParts}...";
                    UpdatePartStatus(i, "Uploading");

                    await Program.ApiService.UploadPartAsync(_uploadId, i, buffer);
                    uploadedParts.Add(i);

                    UpdatePartStatus(i, "Completed");
                    progressBar.Value = (int)((double)i / numParts * 100);
                    lblProgress.Text = $"Progress: {i}/{numParts} parts ({progressBar.Value}%)";
                }

                lblStatus.Text = "Completing upload...";
                await Program.ApiService.CompleteMultipartUploadAsync(_uploadId, uploadedParts);

                lblStatus.Text = "Upload completed successfully!";
                MessageBox.Show("File uploaded successfully!", "Success",
                    MessageBoxButtons.OK, MessageBoxIcon.Information);
                this.DialogResult = DialogResult.OK;
            }
            catch (Exception ex)
            {
                lblStatus.Text = $"Error: {ex.Message}";
                MessageBox.Show($"Upload error: {ex.Message}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally
            {
                btnStart.Enabled = true;
                btnAbort.Enabled = false;
                btnSelectFile.Enabled = true;
            }
        }

        private async void BtnAbort_Click(object? sender, EventArgs e)
        {
            if (string.IsNullOrEmpty(_uploadId))
                return;

            try
            {
                await Program.ApiService.AbortMultipartUploadAsync(_uploadId);
                lblStatus.Text = "Upload aborted";
                MessageBox.Show("Upload aborted", "Info",
                    MessageBoxButtons.OK, MessageBoxIcon.Information);
                this.DialogResult = DialogResult.Cancel;
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error aborting upload: {ex.Message}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void UpdatePartStatus(int partNumber, string status)
        {
            foreach (ListViewItem item in partsList.Items)
            {
                if (item.Text == partNumber.ToString())
                {
                    item.SubItems[2].Text = status;
                    break;
                }
            }
            partsList.Refresh();
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
    }
}