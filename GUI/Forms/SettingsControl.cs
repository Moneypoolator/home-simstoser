using System;
using System.Drawing;
using System.Windows.Forms;
using S3StorageClient.Properties;

namespace S3StorageClient.Forms
{
    public class SettingsControl : UserControl
    {
        private TextBox txtServerUrl;
        private ComboBox cmbTheme;
        private CheckBox chkNotifications;
        private CheckBox chkAutoRefresh;
        private Button btnSave;
        private Button btnReset;
        private Button btnLogout;
        private Label lblUsername;
        private Label lblUserId;
        private Label lblStatus;

        public SettingsControl()
        {
            InitializeComponent();
            LoadSettings();
        }

        private void InitializeComponent()
        {
            this.BackColor = Color.FromArgb(249, 250, 251);
            this.Padding = new Padding(10);
            this.AutoScroll = true;

            // ===== Header =====
            var headerPanel = new Panel
            {
                Dock = DockStyle.Top,
                Height = 60,
                BackColor = Color.White
            };

            var lblTitle = new Label
            {
                Text = "Settings",
                Font = new Font("Segoe UI", 20, FontStyle.Bold),
                ForeColor = Color.FromArgb(17, 24, 39),
                Location = new Point(10, 10),
                AutoSize = true
            };

            var lblSubtitle = new Label
            {
                Text = "Configure your storage client",
                Font = new Font("Segoe UI", 10, FontStyle.Regular),
                ForeColor = Color.FromArgb(107, 114, 128),
                Location = new Point(10, 40),
                AutoSize = true
            };

            headerPanel.Controls.AddRange(new Control[] { lblTitle, lblSubtitle });

            // ===== Content =====
            var contentPanel = new FlowLayoutPanel
            {
                Dock = DockStyle.Fill,
                FlowDirection = FlowDirection.TopDown,
                WrapContents = false,
                AutoScroll = true,
                Padding = new Padding(0, 10, 0, 0)
            };

            // ---- General Settings ----
            var generalCard = CreateSettingsCard("General Settings", 600, 160);
            int yPos = 50;

            var lblServerUrl = new Label
            {
                Text = "Server URL:",
                Font = new Font("Segoe UI", 9, FontStyle.Regular),
                ForeColor = Color.FromArgb(75, 85, 99),
                Location = new Point(15, yPos),
                AutoSize = true
            };
            txtServerUrl = new TextBox
            {
                Location = new Point(15, yPos + 20),
                Size = new Size(550, 30),
                BorderStyle = BorderStyle.FixedSingle,
                Font = new Font("Segoe UI", 10, FontStyle.Regular)
            };
            var lblServerUrlHint = new Label
            {
                Text = "The base URL of the S3-compatible storage server",
                Font = new Font("Segoe UI", 8, FontStyle.Italic),
                ForeColor = Color.FromArgb(156, 163, 175),
                Location = new Point(15, yPos + 48),
                AutoSize = true
            };

            generalCard.Controls.AddRange(new Control[] { lblServerUrl, txtServerUrl, lblServerUrlHint });

            // ---- Appearance ----
            var appearanceCard = CreateSettingsCard("Appearance", 600, 120);
            yPos = 50;

            var lblTheme = new Label
            {
                Text = "Theme:",
                Font = new Font("Segoe UI", 9, FontStyle.Regular),
                ForeColor = Color.FromArgb(75, 85, 99),
                Location = new Point(15, yPos),
                AutoSize = true
            };
            cmbTheme = new ComboBox
            {
                Location = new Point(15, yPos + 20),
                Size = new Size(200, 30),
                DropDownStyle = ComboBoxStyle.DropDownList,
                Font = new Font("Segoe UI", 10, FontStyle.Regular)
            };
            cmbTheme.Items.AddRange(new[] { "Light", "Dark" });
            cmbTheme.SelectedIndex = 0;

            appearanceCard.Controls.AddRange(new Control[] { lblTheme, cmbTheme });

            // ---- Notifications ----
            var notificationsCard = CreateSettingsCard("Notifications", 600, 140);
            yPos = 50;

            chkNotifications = new CheckBox
            {
                Text = "Show notifications",
                Font = new Font("Segoe UI", 10, FontStyle.Regular),
                Location = new Point(15, yPos),
                AutoSize = true,
                Checked = true
            };
            var lblNotifHint = new Label
            {
                Text = "Pop-up notifications for actions",
                Font = new Font("Segoe UI", 8, FontStyle.Italic),
                ForeColor = Color.FromArgb(156, 163, 175),
                Location = new Point(35, yPos + 25),
                AutoSize = true
            };

            chkAutoRefresh = new CheckBox
            {
                Text = "Auto-refresh",
                Font = new Font("Segoe UI", 10, FontStyle.Regular),
                Location = new Point(15, yPos + 50),
                AutoSize = true,
                Checked = true
            };
            var lblRefreshHint = new Label
            {
                Text = "Automatically refresh file lists",
                Font = new Font("Segoe UI", 8, FontStyle.Italic),
                ForeColor = Color.FromArgb(156, 163, 175),
                Location = new Point(35, yPos + 75),
                AutoSize = true
            };

            notificationsCard.Controls.AddRange(new Control[] {
                chkNotifications, lblNotifHint,
                chkAutoRefresh, lblRefreshHint
            });

            // ---- Account ----
            var accountCard = CreateSettingsCard("Account", 600, 160);
            yPos = 50;

            var lblUserLabel = new Label
            {
                Text = "Username:",
                Font = new Font("Segoe UI", 9, FontStyle.Regular),
                ForeColor = Color.FromArgb(75, 85, 99),
                Location = new Point(15, yPos),
                AutoSize = true
            };
            lblUsername = new Label
            {
                Font = new Font("Segoe UI", 10, FontStyle.Bold),
                ForeColor = Color.FromArgb(17, 24, 39),
                Location = new Point(100, yPos),
                AutoSize = true
            };

            var lblUserIdLabel = new Label
            {
                Text = "User ID:",
                Font = new Font("Segoe UI", 9, FontStyle.Regular),
                ForeColor = Color.FromArgb(75, 85, 99),
                Location = new Point(15, yPos + 25),
                AutoSize = true
            };
            lblUserId = new Label
            {
                Font = new Font("Segoe UI", 10, FontStyle.Regular),
                ForeColor = Color.FromArgb(17, 24, 39),
                Location = new Point(100, yPos + 25),
                AutoSize = true
            };

            btnLogout = new Button
            {
                Text = "Logout",
                Font = new Font("Segoe UI", 10, FontStyle.Bold),
                BackColor = Color.FromArgb(220, 38, 38),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                FlatAppearance = { BorderSize = 0 },
                Location = new Point(15, yPos + 60),
                Size = new Size(120, 35),
                Cursor = Cursors.Hand
            };
            btnLogout.Click += BtnLogout_Click;

            accountCard.Controls.AddRange(new Control[] {
                lblUserLabel, lblUsername,
                lblUserIdLabel, lblUserId,
                btnLogout
            });

            // ---- Actions ----
            var actionsCard = CreateSettingsCard("Actions", 600, 100);
            yPos = 50;

            btnSave = new Button
            {
                Text = "Save Settings",
                Font = new Font("Segoe UI", 10, FontStyle.Bold),
                BackColor = Color.FromArgb(37, 99, 235),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                FlatAppearance = { BorderSize = 0 },
                Location = new Point(15, yPos),
                Size = new Size(130, 35),
                Cursor = Cursors.Hand
            };
            btnSave.Click += BtnSave_Click;

            btnReset = new Button
            {
                Text = "Reset Settings",
                Font = new Font("Segoe UI", 10, FontStyle.Regular),
                BackColor = Color.FromArgb(107, 114, 128),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                FlatAppearance = { BorderSize = 0 },
                Location = new Point(155, yPos),
                Size = new Size(130, 35),
                Cursor = Cursors.Hand
            };
            btnReset.Click += BtnReset_Click;

            actionsCard.Controls.AddRange(new Control[] { btnSave, btnReset });

            // ---- System Info ----
            var sysInfoCard = CreateSettingsCard("System Information", 600, 140);
            yPos = 50;

            AddSysInfoRow(sysInfoCard, "Version", "1.0.0", ref yPos);
            AddSysInfoRow(sysInfoCard, "Build", "2026.03.20", ref yPos);
            AddSysInfoRow(sysInfoCard, "Platform", ".NET 8.0 WinForms", ref yPos);

            contentPanel.Controls.AddRange(new Control[] {
                generalCard, appearanceCard, notificationsCard,
                accountCard, actionsCard, sysInfoCard
            });

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

            this.Controls.Add(contentPanel);
            this.Controls.Add(headerPanel);
            this.Controls.Add(lblStatus);
        }

        private Panel CreateSettingsCard(string title, int width, int height)
        {
            var card = new Panel
            {
                Size = new Size(width, height),
                BackColor = Color.White,
                Margin = new Padding(0, 0, 0, 10)
            };

            var headerBar = new Panel
            {
                Height = 45,
                Width = width,
                BackColor = Color.White
            };

            var lblTitle = new Label
            {
                Text = title,
                Font = new Font("Segoe UI", 12, FontStyle.Bold),
                ForeColor = Color.FromArgb(17, 24, 39),
                Location = new Point(15, 12),
                AutoSize = true
            };

            headerBar.Controls.Add(lblTitle);
            card.Controls.Add(headerBar);

            return card;
        }

        private void AddSysInfoRow(Panel parent, string label, string value, ref int yPos)
        {
            var lblLabel = new Label
            {
                Text = label,
                Font = new Font("Segoe UI", 9, FontStyle.Regular),
                ForeColor = Color.FromArgb(107, 114, 128),
                Location = new Point(15, yPos),
                AutoSize = true
            };

            var lblValue = new Label
            {
                Text = value,
                Font = new Font("Segoe UI", 9, FontStyle.Bold),
                ForeColor = Color.FromArgb(17, 24, 39),
                Location = new Point(120, yPos),
                AutoSize = true
            };

            parent.Controls.AddRange(new Control[] { lblLabel, lblValue });
            yPos += 25;
        }

        private void LoadSettings()
        {
            txtServerUrl.Text = Settings.Default.ServerUrl;
            cmbTheme.SelectedItem = Settings.Default.Theme ?? "Light";
            chkNotifications.Checked = Settings.Default.Notifications;
            chkAutoRefresh.Checked = Settings.Default.AutoRefresh;

            lblUsername.Text = Settings.Default.Username;
            lblUserId.Text = Settings.Default.UserId;
        }

        private void BtnSave_Click(object? sender, EventArgs e)
        {
            try
            {
                Settings.Default.ServerUrl = txtServerUrl.Text.Trim();
                Settings.Default.Theme = cmbTheme.SelectedItem?.ToString() ?? "Light";
                Settings.Default.Notifications = chkNotifications.Checked;
                Settings.Default.AutoRefresh = chkAutoRefresh.Checked;
                Settings.Default.Save();

                // Update API service base URL
                Program.ApiService.SetBaseUrl(txtServerUrl.Text.Trim());

                lblStatus.Text = "Settings saved successfully";
                MessageBox.Show("Settings saved successfully", "Settings",
                    MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                lblStatus.Text = $"Error saving settings: {ex.Message}";
                MessageBox.Show($"Error saving settings: {ex.Message}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void BtnReset_Click(object? sender, EventArgs e)
        {
            var result = MessageBox.Show("Reset all settings to default values?",
                "Confirm Reset", MessageBoxButtons.YesNo, MessageBoxIcon.Question);

            if (result == DialogResult.Yes)
            {
                Settings.Default.Reset();
                LoadSettings();
                lblStatus.Text = "Settings reset to defaults";
                MessageBox.Show("Settings reset to defaults", "Settings",
                    MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
        }

        private void BtnLogout_Click(object? sender, EventArgs e)
        {
            var result = MessageBox.Show("Are you sure you want to logout?",
                "Confirm Logout", MessageBoxButtons.YesNo, MessageBoxIcon.Question);

            if (result == DialogResult.Yes)
            {
                Program.AuthService.Logout();

                // Close the main form and show login
                var mainForm = this.FindForm();
                if (mainForm != null)
                {
                    mainForm.Close();
                }

                var loginForm = new LoginForm();
                loginForm.ShowDialog();

                // If login was successful, the application will restart via Program
                Application.Exit();
            }
        }
    }
}