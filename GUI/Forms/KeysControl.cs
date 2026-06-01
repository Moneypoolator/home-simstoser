using System;
using System.Drawing;
using System.Windows.Forms;
using S3StorageClient.Models;

namespace S3StorageClient.Forms
{
    public class KeysControl : UserControl
    {
        private ListView keysList;
        private Button btnCreate;
        private Button btnActivate;
        private Button btnDeactivate;
        private Button btnDelete;
        private Button btnRefresh;
        private Label lblStatus;

        public KeysControl()
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

            btnCreate = CreateToolbarButton("Create Key", 10);
            btnCreate.Click += BtnCreate_Click;

            btnActivate = CreateToolbarButton("Activate", 110);
            btnActivate.Click += BtnActivate_Click;

            btnDeactivate = CreateToolbarButton("Deactivate", 200);
            btnDeactivate.Click += BtnDeactivate_Click;

            btnDelete = CreateToolbarButton("Delete", 300);
            btnDelete.Click += BtnDelete_Click;

            btnRefresh = CreateToolbarButton("Refresh", 390);
            btnRefresh.Click += (s, e) => LoadKeys();

            toolbar.Controls.AddRange(new[] { btnCreate, btnActivate, btnDeactivate, btnDelete, btnRefresh });

            // Keys list
            keysList = new ListView
            {
                View = View.Details,
                FullRowSelect = true,
                GridLines = true,
                Dock = DockStyle.Fill
            };
            keysList.Columns.Add("Access Key ID", 200);
            keysList.Columns.Add("User", 120);
            keysList.Columns.Add("Status", 80);
            keysList.Columns.Add("Created", 150);

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

            this.Controls.Add(keysList);
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
                Size = new Size(90, 30),
                Location = new Point(x, 10),
                Cursor = Cursors.Hand
            };
        }

        public async void LoadKeys()
        {
            try
            {
                lblStatus.Text = "Loading keys...";
                var data = await Program.ApiService.ListKeysAsync();
                var keys = data.Keys ?? new List<AccessKey>();

                keysList.Items.Clear();
                foreach (var key in keys)
                {
                    var item = new ListViewItem(key.AccessKeyId);
                    item.SubItems.Add(key.UserName);
                    item.SubItems.Add(key.IsActive ? "Active" : "Inactive");
                    item.SubItems.Add(FormatDate(key.CreatedAt));
                    item.Tag = key;
                    keysList.Items.Add(item);
                }

                lblStatus.Text = $"Loaded {keys.Count} keys";
            }
            catch (Exception ex)
            {
                lblStatus.Text = $"Error: {ex.Message}";
                MessageBox.Show($"Error loading keys: {ex.Message}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void BtnCreate_Click(object? sender, EventArgs e)
        {
            var form = new CreateKeyDialog();
            if (form.ShowDialog() == DialogResult.OK)
            {
                LoadKeys();
            }
        }

        private async void BtnActivate_Click(object? sender, EventArgs e)
        {
            if (keysList.SelectedItems.Count == 0) return;
            var key = (AccessKey)keysList.SelectedItems[0].Tag;

            try
            {
                await Program.ApiService.ActivateKeyAsync(key.AccessKeyId);
                LoadKeys();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error activating key: {ex.Message}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private async void BtnDeactivate_Click(object? sender, EventArgs e)
        {
            if (keysList.SelectedItems.Count == 0) return;
            var key = (AccessKey)keysList.SelectedItems[0].Tag;

            try
            {
                await Program.ApiService.DeactivateKeyAsync(key.AccessKeyId);
                LoadKeys();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error deactivating key: {ex.Message}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private async void BtnDelete_Click(object? sender, EventArgs e)
        {
            if (keysList.SelectedItems.Count == 0) return;
            var key = (AccessKey)keysList.SelectedItems[0].Tag;

            var result = MessageBox.Show($"Are you sure you want to delete key '{key.AccessKeyId}'?",
                "Confirm Delete", MessageBoxButtons.YesNo, MessageBoxIcon.Question);

            if (result == DialogResult.Yes)
            {
                try
                {
                    await Program.ApiService.DeleteKeyAsync(key.AccessKeyId);
                    LoadKeys();
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Error deleting key: {ex.Message}", "Error",
                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }

        private static string FormatDate(string dateString)
        {
            if (DateTime.TryParse(dateString, out var dt))
                return dt.ToString("MMM dd, yyyy HH:mm");
            return dateString;
        }
    }

    public class CreateKeyDialog : Form
    {
        private ComboBox cmbUser;
        private Button btnSave;
        private Button btnCancel;

        public CreateKeyDialog()
        {
            InitializeComponent();
            this.StartPosition = FormStartPosition.CenterParent;
            this.ClientSize = new Size(400, 200);
            this.Text = "Create Access Key";
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
        }

        private void InitializeComponent()
        {
            this.BackColor = Color.White;
            this.Padding = new Padding(20);

            var lblUser = new Label { Text = "Select User:", Location = new Point(20, 20), AutoSize = true };

            cmbUser = new ComboBox
            {
                Location = new Point(20, 40),
                Size = new Size(340, 30),
                DropDownStyle = ComboBoxStyle.DropDownList
            };

            // Load users
            LoadUsersAsync();

            btnSave = new Button
            {
                Text = "Create",
                Font = new Font("Segoe UI", 10, FontStyle.Bold),
                BackColor = Color.FromArgb(37, 99, 235),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                FlatAppearance = { BorderSize = 0 },
                Location = new Point(20, 90),
                Size = new Size(100, 30),
                Cursor = Cursors.Hand
            };
            btnSave.Click += BtnSave_Click;

            btnCancel = new Button
            {
                Text = "Cancel",
                Font = new Font("Segoe UI", 10, FontStyle.Regular),
                Location = new Point(130, 90),
                Size = new Size(100, 30),
                Cursor = Cursors.Hand
            };
            btnCancel.Click += (s, e) => this.DialogResult = DialogResult.Cancel;

            this.Controls.AddRange(new Control[] { lblUser, cmbUser, btnSave, btnCancel });
        }

        private async void LoadUsersAsync()
        {
            try
            {
                var data = await Program.ApiService.ListUsersAsync();
                foreach (var user in data.Users.Where(u => u.IsActive))
                {
                    cmbUser.Items.Add(user.Username);
                }
                if (cmbUser.Items.Count > 0)
                    cmbUser.SelectedIndex = 0;
            }
            catch { }
        }

        private async void BtnSave_Click(object? sender, EventArgs e)
        {
            if (cmbUser.SelectedItem == null)
            {
                MessageBox.Show("Please select a user", "Validation Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            try
            {
                var newKey = await Program.ApiService.CreateKeyAsync(cmbUser.SelectedItem.ToString()!);

                var secretMsg = $"Access Key created successfully!\n\n" +
                    $"Access Key ID: {newKey.AccessKeyId}\n" +
                    $"Secret Key: {newKey.SecretAccessKey}\n\n" +
                    "IMPORTANT: Copy the secret key now - it will not be shown again!";

                MessageBox.Show(secretMsg, "Key Created", MessageBoxButtons.OK, MessageBoxIcon.Information);

                this.DialogResult = DialogResult.OK;
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error creating key: {ex.Message}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }
}