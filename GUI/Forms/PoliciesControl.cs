using System;
using System.Drawing;
using System.Windows.Forms;
using S3StorageClient.Models;

namespace S3StorageClient.Forms
{
    public class PoliciesControl : UserControl
    {
        private ListView policiesList;
        private Button btnCreate;
        private Button btnEdit;
        private Button btnDelete;
        private Button btnRefresh;
        private Label lblStatus;

        public PoliciesControl()
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

            btnCreate = CreateToolbarButton("Create Policy", 10);
            btnCreate.Click += BtnCreate_Click;

            btnEdit = CreateToolbarButton("Edit", 110);
            btnEdit.Click += BtnEdit_Click;

            btnDelete = CreateToolbarButton("Delete", 200);
            btnDelete.Click += BtnDelete_Click;

            btnRefresh = CreateToolbarButton("Refresh", 290);
            btnRefresh.Click += (s, e) => LoadPolicies();

            toolbar.Controls.AddRange(new[] { btnCreate, btnEdit, btnDelete, btnRefresh });

            // Policies list
            policiesList = new ListView
            {
                View = View.Details,
                FullRowSelect = true,
                GridLines = true,
                Dock = DockStyle.Fill
            };
            policiesList.Columns.Add("Name", 200);
            policiesList.Columns.Add("Description", 300);
            policiesList.Columns.Add("Permissions", 200);
            policiesList.Columns.Add("Created", 150);

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

            this.Controls.Add(policiesList);
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

        public async void LoadPolicies()
        {
            try
            {
                lblStatus.Text = "Loading policies...";
                var data = await Program.ApiService.ListPoliciesAsync();
                var policies = data.Policies ?? new List<AccessPolicy>();

                policiesList.Items.Clear();
                foreach (var policy in policies)
                {
                    var item = new ListViewItem(policy.Name);
                    item.SubItems.Add(policy.Description);
                    item.SubItems.Add(FormatPermissions(policy.Permissions));
                    item.SubItems.Add(FormatDate(policy.CreatedAt));
                    item.Tag = policy;
                    policiesList.Items.Add(item);
                }

                lblStatus.Text = $"Loaded {policies.Count} policies";
            }
            catch (Exception ex)
            {
                lblStatus.Text = $"Error: {ex.Message}";
                MessageBox.Show($"Error loading policies: {ex.Message}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void BtnCreate_Click(object? sender, EventArgs e)
        {
            var form = new PolicyFormDialog();
            if (form.ShowDialog() == DialogResult.OK)
            {
                LoadPolicies();
            }
        }

        private void BtnEdit_Click(object? sender, EventArgs e)
        {
            if (policiesList.SelectedItems.Count == 0) return;
            var policy = (AccessPolicy)policiesList.SelectedItems[0].Tag;

            var form = new PolicyFormDialog(policy);
            if (form.ShowDialog() == DialogResult.OK)
            {
                LoadPolicies();
            }
        }

        private async void BtnDelete_Click(object? sender, EventArgs e)
        {
            if (policiesList.SelectedItems.Count == 0) return;
            var policy = (AccessPolicy)policiesList.SelectedItems[0].Tag;

            var result = MessageBox.Show($"Are you sure you want to delete policy '{policy.Name}'?",
                "Confirm Delete", MessageBoxButtons.YesNo, MessageBoxIcon.Question);

            if (result == DialogResult.Yes)
            {
                try
                {
                    await Program.ApiService.DeletePolicyAsync(policy.PolicyId);
                    LoadPolicies();
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Error deleting policy: {ex.Message}", "Error",
                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }

        private static string FormatPermissions(List<Permission> permissions)
        {
            if (permissions == null || permissions.Count == 0)
                return "None";

            var parts = new List<string>();
            foreach (var perm in permissions)
            {
                var allowDeny = perm.Allow ? "✓" : "✗";
                parts.Add($"{perm.Type} {allowDeny} ({perm.ResourcePattern})");
            }
            return string.Join(", ", parts);
        }

        private static string FormatDate(string dateString)
        {
            if (DateTime.TryParse(dateString, out var dt))
                return dt.ToString("MMM dd, yyyy HH:mm");
            return dateString;
        }
    }

    public class PolicyFormDialog : Form
    {
        private TextBox txtName;
        private TextBox txtDescription;
        private Panel permissionsPanel;
        private Button btnAddPermission;
        private Button btnSave;
        private Button btnCancel;
        private List<PermissionRow> permissionRows;
        private AccessPolicy? _existingPolicy;

        public PolicyFormDialog(AccessPolicy? existingPolicy = null)
        {
            _existingPolicy = existingPolicy;
            permissionRows = new List<PermissionRow>();
            InitializeComponent();
            this.StartPosition = FormStartPosition.CenterParent;
            this.ClientSize = new Size(600, 500);
            this.Text = existingPolicy != null ? "Edit Policy" : "Create Policy";
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;

            if (existingPolicy != null)
            {
                txtName.Text = existingPolicy.Name;
                txtDescription.Text = existingPolicy.Description;
                foreach (var perm in existingPolicy.Permissions)
                {
                    AddPermissionRow(perm.Type, perm.ResourcePattern, perm.Allow);
                }
            }
        }

        private void InitializeComponent()
        {
            this.BackColor = Color.White;
            this.Padding = new Padding(20);

            int yPos = 20;

            // Name
            var lblName = new Label { Text = "Policy Name *:", Location = new Point(20, yPos), AutoSize = true };
            txtName = new TextBox
            {
                Location = new Point(20, yPos + 20),
                Size = new Size(540, 30),
                BorderStyle = BorderStyle.FixedSingle
            };
            yPos += 60;

            // Description
            var lblDesc = new Label { Text = "Description:", Location = new Point(20, yPos), AutoSize = true };
            txtDescription = new TextBox
            {
                Location = new Point(20, yPos + 20),
                Size = new Size(540, 60),
                BorderStyle = BorderStyle.FixedSingle,
                Multiline = true,
                AcceptsReturn = true
            };
            yPos += 100;

            // Permissions header
            var lblPermissions = new Label { Text = "Permissions:", Location = new Point(20, yPos), AutoSize = true };
            btnAddPermission = new Button
            {
                Text = "+ Add Permission",
                Font = new Font("Segoe UI", 9, FontStyle.Regular),
                BackColor = Color.FromArgb(37, 99, 235),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                FlatAppearance = { BorderSize = 0 },
                Location = new Point(120, yPos - 2),
                Size = new Size(130, 25),
                Cursor = Cursors.Hand
            };
            btnAddPermission.Click += (s, e) => AddPermissionRow("READ", "*", true);
            yPos += 30;

            // Permissions panel (scrollable)
            permissionsPanel = new Panel
            {
                Location = new Point(20, yPos),
                Size = new Size(540, 250),
                AutoScroll = true,
                BorderStyle = BorderStyle.None
            };
            yPos += 260;

            // Buttons
            btnSave = new Button
            {
                Text = _existingPolicy != null ? "Save Changes" : "Create",
                Font = new Font("Segoe UI", 10, FontStyle.Bold),
                BackColor = Color.FromArgb(37, 99, 235),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                FlatAppearance = { BorderSize = 0 },
                Location = new Point(20, yPos),
                Size = new Size(120, 30),
                Cursor = Cursors.Hand
            };
            btnSave.Click += BtnSave_Click;

            btnCancel = new Button
            {
                Text = "Cancel",
                Font = new Font("Segoe UI", 10, FontStyle.Regular),
                Location = new Point(150, yPos),
                Size = new Size(100, 30),
                Cursor = Cursors.Hand
            };
            btnCancel.Click += (s, e) => this.DialogResult = DialogResult.Cancel;

            this.Controls.AddRange(new Control[] {
                lblName, txtName,
                lblDesc, txtDescription,
                lblPermissions, btnAddPermission,
                permissionsPanel,
                btnSave, btnCancel
            });

            // Add initial empty permission row if no existing policy
            if (_existingPolicy == null)
            {
                AddPermissionRow("READ", "*", true);
            }
        }

        private void AddPermissionRow(string type, string pattern, bool allow)
        {
            var row = new PermissionRow(permissionsPanel, permissionRows.Count, type, pattern, allow);
            row.RemoveRequested += (s, e) =>
            {
                permissionRows.Remove(row);
                ReflowPermissionRows();
            };
            permissionRows.Add(row);
            ReflowPermissionRows();
        }

        private void ReflowPermissionRows()
        {
            int y = 0;
            foreach (var row in permissionRows)
            {
                row.Location = new Point(0, y);
                row.Visible = true;
                y += row.Height + 5;
            }
        }

        private async void BtnSave_Click(object? sender, EventArgs e)
        {
            if (string.IsNullOrEmpty(txtName.Text.Trim()))
            {
                MessageBox.Show("Policy name is required", "Validation Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            var permissions = new List<Permission>();
            foreach (var row in permissionRows)
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
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            try
            {
                if (_existingPolicy != null)
                {
                    await Program.ApiService.UpdatePolicyAsync(
                        _existingPolicy.PolicyId,
                        txtName.Text.Trim(),
                        txtDescription.Text.Trim(),
                        permissions
                    );
                }
                else
                {
                    await Program.ApiService.CreatePolicyAsync(
                        txtName.Text.Trim(),
                        txtDescription.Text.Trim(),
                        permissions
                    );
                }
                this.DialogResult = DialogResult.OK;
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error saving policy: {ex.Message}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }

    public class PermissionRow : Panel
    {
        private ComboBox cmbType;
        private TextBox txtPattern;
        private CheckBox chkAllow;
        private Button btnRemove;

        public string SelectedType => cmbType.SelectedItem?.ToString() ?? "READ";
        public string ResourcePattern => txtPattern.Text.Trim();
        public bool Allow => chkAllow.Checked;

        public event EventHandler? RemoveRequested;

        public PermissionRow(Control parent, int index, string type, string pattern, bool allow)
        {
            this.Parent = parent;
            this.Size = new Size(520, 35);
            this.BackColor = Color.FromArgb(243, 244, 246);

            cmbType = new ComboBox
            {
                Location = new Point(5, 5),
                Size = new Size(100, 25),
                DropDownStyle = ComboBoxStyle.DropDownList
            };
            cmbType.Items.AddRange(new[] { "READ", "WRITE", "DELETE", "LIST", "MANAGE_ACL" });
            cmbType.SelectedItem = type;

            txtPattern = new TextBox
            {
                Location = new Point(110, 5),
                Size = new Size(200, 25),
                Text = pattern,
                BorderStyle = BorderStyle.FixedSingle
            };

            chkAllow = new CheckBox
            {
                Text = "Allow",
                Location = new Point(320, 5),
                Size = new Size(80, 25),
                Checked = allow
            };

            btnRemove = new Button
            {
                Text = "✕",
                Font = new Font("Segoe UI", 8, FontStyle.Regular),
                ForeColor = Color.Red,
                FlatStyle = FlatStyle.Flat,
                FlatAppearance = { BorderSize = 0 },
                Location = new Point(410, 5),
                Size = new Size(25, 25),
                Cursor = Cursors.Hand
            };
            btnRemove.Click += (s, e) => RemoveRequested?.Invoke(this, EventArgs.Empty);

            this.Controls.AddRange(new Control[] { cmbType, txtPattern, chkAllow, btnRemove });
        }
    }
}