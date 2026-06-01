using System;
using System.Drawing;
using System.Windows.Forms;
using S3StorageClient.Models;

namespace S3StorageClient.Forms
{
    public class UsersControl : UserControl
    {
        private ListView usersList;
        private Button btnCreate;
        private Button btnActivate;
        private Button btnDeactivate;
        private Button btnDelete;
        private Button btnRefresh;
        private Label lblStatus;

        public UsersControl()
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

            btnCreate = CreateToolbarButton("Create User", 10);
            btnCreate.Click += BtnCreate_Click;

            btnActivate = CreateToolbarButton("Activate", 110);
            btnActivate.Click += BtnActivate_Click;

            btnDeactivate = CreateToolbarButton("Deactivate", 200);
            btnDeactivate.Click += BtnDeactivate_Click;

            btnDelete = CreateToolbarButton("Delete", 300);
            btnDelete.Click += BtnDelete_Click;

            btnRefresh = CreateToolbarButton("Refresh", 390);
            btnRefresh.Click += (s, e) => LoadUsers();

            toolbar.Controls.AddRange(new[] { btnCreate, btnActivate, btnDeactivate, btnDelete, btnRefresh });

            // Users list
            usersList = new ListView
            {
                View = View.Details,
                FullRowSelect = true,
                GridLines = true,
                Dock = DockStyle.Fill
            };
            usersList.Columns.Add("Username", 150);
            usersList.Columns.Add("Email", 200);
            usersList.Columns.Add("Role", 100);
            usersList.Columns.Add("Status", 80);
            usersList.Columns.Add("Last Login", 150);
            usersList.Columns.Add("Created", 150);

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

            this.Controls.Add(usersList);
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

        public async void LoadUsers()
        {
            try
            {
                lblStatus.Text = "Loading users...";
                var data = await Program.ApiService.ListUsersAsync();
                var users = data.Users ?? new List<User>();

                usersList.Items.Clear();
                foreach (var user in users)
                {
                    var item = new ListViewItem(user.Username);
                    item.SubItems.Add(user.Email);
                    item.SubItems.Add(GetRoleDisplayName(user.Role));
                    item.SubItems.Add(user.IsActive ? "Active" : "Inactive");
                    item.SubItems.Add(FormatDate(user.LastLogin));
                    item.SubItems.Add(FormatDate(user.CreatedAt));
                    item.Tag = user;
                    usersList.Items.Add(item);
                }

                lblStatus.Text = $"Loaded {users.Count} users";
            }
            catch (Exception ex)
            {
                lblStatus.Text = $"Error: {ex.Message}";
                MessageBox.Show($"Error loading users: {ex.Message}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void BtnCreate_Click(object? sender, EventArgs e)
        {
            var form = new UserFormDialog();
            if (form.ShowDialog() == DialogResult.OK)
            {
                LoadUsers();
            }
        }

        private async void BtnActivate_Click(object? sender, EventArgs e)
        {
            if (usersList.SelectedItems.Count == 0) return;
            var user = (User)usersList.SelectedItems[0].Tag;

            try
            {
                await Program.ApiService.ActivateUserAsync(user.UserId);
                LoadUsers();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error activating user: {ex.Message}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private async void BtnDeactivate_Click(object? sender, EventArgs e)
        {
            if (usersList.SelectedItems.Count == 0) return;
            var user = (User)usersList.SelectedItems[0].Tag;

            try
            {
                await Program.ApiService.DeactivateUserAsync(user.UserId);
                LoadUsers();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error deactivating user: {ex.Message}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private async void BtnDelete_Click(object? sender, EventArgs e)
        {
            if (usersList.SelectedItems.Count == 0) return;
            var user = (User)usersList.SelectedItems[0].Tag;

            var result = MessageBox.Show($"Are you sure you want to delete user '{user.Username}'?",
                "Confirm Delete", MessageBoxButtons.YesNo, MessageBoxIcon.Question);

            if (result == DialogResult.Yes)
            {
                try
                {
                    await Program.ApiService.DeleteUserAsync(user.UserId);
                    LoadUsers();
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Error deleting user: {ex.Message}", "Error",
                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }

        private static string GetRoleDisplayName(string role)
        {
            return role switch
            {
                "ADMIN" => "Administrator",
                "MANAGER" => "Manager",
                "CONTRIBUTOR" => "Contributor",
                "VIEWER" => "Viewer",
                "GUEST" => "Guest",
                _ => role
            };
        }

        private static string FormatDate(string? dateString)
        {
            if (string.IsNullOrEmpty(dateString)) return "Never";
            if (DateTime.TryParse(dateString, out var dt))
                return dt.ToString("MMM dd, yyyy HH:mm");
            return dateString;
        }
    }

    public class UserFormDialog : Form
    {
        private TextBox txtUsername;
        private TextBox txtEmail;
        private ComboBox cmbRole;
        private Button btnSave;
        private Button btnCancel;

        public UserFormDialog()
        {
            InitializeComponent();
            this.StartPosition = FormStartPosition.CenterParent;
            this.ClientSize = new Size(400, 280);
            this.Text = "Create User";
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
        }

        private void InitializeComponent()
        {
            this.BackColor = Color.White;
            this.Padding = new Padding(20);

            var lblUsername = new Label { Text = "Username:", Location = new Point(20, 20), AutoSize = true };
            txtUsername = new TextBox { Location = new Point(20, 40), Size = new Size(340, 30), BorderStyle = BorderStyle.FixedSingle };

            var lblEmail = new Label { Text = "Email:", Location = new Point(20, 80), AutoSize = true };
            txtEmail = new TextBox { Location = new Point(20, 100), Size = new Size(340, 30), BorderStyle = BorderStyle.FixedSingle };

            var lblRole = new Label { Text = "Role:", Location = new Point(20, 140), AutoSize = true };
            cmbRole = new ComboBox
            {
                Location = new Point(20, 160),
                Size = new Size(340, 30),
                DropDownStyle = ComboBoxStyle.DropDownList
            };
            cmbRole.Items.AddRange(new[] { "ADMIN", "MANAGER", "CONTRIBUTOR", "VIEWER", "GUEST" });
            cmbRole.SelectedIndex = 3;

            btnSave = new Button
            {
                Text = "Create",
                Font = new Font("Segoe UI", 10, FontStyle.Bold),
                BackColor = Color.FromArgb(37, 99, 235),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                FlatAppearance = { BorderSize = 0 },
                Location = new Point(20, 210),
                Size = new Size(100, 30),
                Cursor = Cursors.Hand
            };
            btnSave.Click += BtnSave_Click;

            btnCancel = new Button
            {
                Text = "Cancel",
                Font = new Font("Segoe UI", 10, FontStyle.Regular),
                Location = new Point(130, 210),
                Size = new Size(100, 30),
                Cursor = Cursors.Hand
            };
            btnCancel.Click += (s, e) => this.DialogResult = DialogResult.Cancel;

            this.Controls.AddRange(new Control[] {
                lblUsername, txtUsername,
                lblEmail, txtEmail,
                lblRole, cmbRole,
                btnSave, btnCancel
            });
        }

        private async void BtnSave_Click(object? sender, EventArgs e)
        {
            if (string.IsNullOrEmpty(txtUsername.Text) || string.IsNullOrEmpty(txtEmail.Text))
            {
                MessageBox.Show("Please fill in all fields", "Validation Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            try
            {
                await Program.ApiService.CreateUserAsync(
                    txtUsername.Text.Trim(),
                    txtEmail.Text.Trim(),
                    cmbRole.SelectedItem?.ToString() ?? "VIEWER"
                );
                this.DialogResult = DialogResult.OK;
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error creating user: {ex.Message}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }
}