using System;
using System.Drawing;
using System.Windows.Forms;

namespace S3StorageClient.Forms
{
    public partial class MainForm : Form
    {
        private Panel sidebarPanel;
        private Panel contentPanel;
        private Panel headerPanel;
        private Label lblTitle;
        private Label lblUsername;
        private Button btnLogout;
        private Button btnDashboard;
        private Button btnFiles;
        private Button btnUsers;
        private Button btnKeys;
        private Button btnPolicies;
        private Button btnMonitoring;
        private Button btnSettings;

        // Content controls
        private DashboardControl dashboardControl;
        private FilesControl filesControl;
        private UsersControl usersControl;
        private KeysControl keysControl;
        private PoliciesControl policiesControl;
        private MonitoringControl monitoringControl;
        private SettingsControl settingsControl;

        private Control? currentContent;

        public MainForm()
        {
            InitializeComponent();
            this.StartPosition = FormStartPosition.CenterScreen;
            this.WindowState = FormWindowState.Maximized;
            this.Text = "S3 Storage Client";
            this.MinimumSize = new Size(1024, 700);

            ShowDashboard();
        }

        private void InitializeComponent()
        {
            this.Icon = SystemIcons.Application;
            this.BackColor = Color.FromArgb(249, 250, 251);

            // Sidebar
            sidebarPanel = new Panel
            {
                Width = 220,
                Dock = DockStyle.Left,
                BackColor = Color.FromArgb(31, 41, 55)
            };

            var logoLabel = new Label
            {
                Text = "S3 Storage",
                Font = new Font("Segoe UI", 16, FontStyle.Bold),
                ForeColor = Color.White,
                TextAlign = ContentAlignment.MiddleCenter,
                Height = 60,
                Dock = DockStyle.Top,
                Padding = new Padding(0, 15, 0, 0)
            };

            var navPanel = new Panel
            {
                Dock = DockStyle.Fill,
                Padding = new Padding(10, 10, 10, 0)
            };

            btnDashboard = CreateNavButton("Dashboard", 0);
            btnFiles = CreateNavButton("Files", 1);
            btnUsers = CreateNavButton("Users", 2);
            btnKeys = CreateNavButton("Access Keys", 3);
            btnPolicies = CreateNavButton("Policies", 4);
            btnMonitoring = CreateNavButton("Monitoring", 5);
            btnSettings = CreateNavButton("Settings", 6);

            btnDashboard.Click += (s, e) => ShowDashboard();
            btnFiles.Click += (s, e) => ShowFiles();
            btnUsers.Click += (s, e) => ShowUsers();
            btnKeys.Click += (s, e) => ShowKeys();
            btnPolicies.Click += (s, e) => ShowPolicies();
            btnMonitoring.Click += (s, e) => ShowMonitoring();
            btnSettings.Click += (s, e) => ShowSettings();

            navPanel.Controls.AddRange(new Control[] {
                btnSettings, btnMonitoring, btnPolicies, btnKeys, btnUsers, btnFiles, btnDashboard
            });

            sidebarPanel.Controls.Add(navPanel);
            sidebarPanel.Controls.Add(logoLabel);

            // Header
            headerPanel = new Panel
            {
                Height = 56,
                Dock = DockStyle.Top,
                BackColor = Color.White
            };

            lblTitle = new Label
            {
                Text = "Dashboard",
                Font = new Font("Segoe UI", 18, FontStyle.Bold),
                ForeColor = Color.FromArgb(31, 41, 55),
                Location = new Point(20, 12),
                AutoSize = true
            };

            lblUsername = new Label
            {
                Text = Program.AuthService.Username ?? "",
                Font = new Font("Segoe UI", 10, FontStyle.Regular),
                ForeColor = Color.FromArgb(107, 114, 128),
                TextAlign = ContentAlignment.MiddleRight,
                Location = new Point(headerPanel.Width - 250, 18),
                Size = new Size(150, 20),
                Anchor = AnchorStyles.Top | AnchorStyles.Right
            };

            btnLogout = new Button
            {
                Text = "Sign Out",
                Font = new Font("Segoe UI", 9, FontStyle.Regular),
                ForeColor = Color.FromArgb(55, 65, 81),
                BackColor = Color.FromArgb(243, 244, 246),
                FlatStyle = FlatStyle.Flat,
                FlatAppearance = { BorderSize = 0 },
                Size = new Size(80, 30),
                Location = new Point(headerPanel.Width - 90, 13),
                Anchor = AnchorStyles.Top | AnchorStyles.Right,
                Cursor = Cursors.Hand
            };
            btnLogout.Click += BtnLogout_Click;

            headerPanel.Controls.Add(lblTitle);
            headerPanel.Controls.Add(lblUsername);
            headerPanel.Controls.Add(btnLogout);

            // Content panel
            contentPanel = new Panel
            {
                Dock = DockStyle.Fill,
                Padding = new Padding(20)
            };

            // Create content controls
            dashboardControl = new DashboardControl { Dock = DockStyle.Fill };
            filesControl = new FilesControl { Dock = DockStyle.Fill };
            usersControl = new UsersControl { Dock = DockStyle.Fill };
            keysControl = new KeysControl { Dock = DockStyle.Fill };
            policiesControl = new PoliciesControl { Dock = DockStyle.Fill };
            monitoringControl = new MonitoringControl { Dock = DockStyle.Fill };
            settingsControl = new SettingsControl { Dock = DockStyle.Fill };

            this.Controls.Add(contentPanel);
            this.Controls.Add(headerPanel);
            this.Controls.Add(sidebarPanel);
        }

        private Button CreateNavButton(string text, int index)
        {
            var btn = new Button
            {
                Text = "  " + text,
                Font = new Font("Segoe UI", 11, FontStyle.Regular),
                ForeColor = Color.FromArgb(209, 213, 219),
                BackColor = Color.Transparent,
                FlatStyle = FlatStyle.Flat,
                FlatAppearance = { BorderSize = 0 },
                Height = 40,
                Dock = DockStyle.Top,
                TextAlign = ContentAlignment.MiddleLeft,
                Padding = new Padding(15, 0, 0, 0),
                Cursor = Cursors.Hand
            };
            btn.MouseEnter += (s, e) => { if (btn.BackColor == Color.Transparent) btn.BackColor = Color.FromArgb(55, 65, 81); };
            btn.MouseLeave += (s, e) => { if (btn.BackColor == Color.FromArgb(55, 65, 81)) btn.BackColor = Color.Transparent; };
            return btn;
        }

        private void HighlightNavButton(Button activeBtn)
        {
            foreach (var btn in new[] { btnDashboard, btnFiles, btnUsers, btnKeys, btnPolicies, btnMonitoring, btnSettings })
            {
                btn.BackColor = Color.Transparent;
                btn.Font = new Font("Segoe UI", 11, FontStyle.Regular);
            }
            activeBtn.BackColor = Color.FromArgb(37, 99, 235);
            activeBtn.Font = new Font("Segoe UI", 11, FontStyle.Bold);
        }

        private void ShowContent(Control control, string title)
        {
            if (currentContent != null)
                contentPanel.Controls.Remove(currentContent);

            currentContent = control;
            contentPanel.Controls.Add(control);
            lblTitle.Text = title;
        }

        public void ShowDashboard()
        {
            HighlightNavButton(btnDashboard);
            ShowContent(dashboardControl, "Dashboard");
            dashboardControl.LoadData();
        }

        public void ShowFiles()
        {
            HighlightNavButton(btnFiles);
            ShowContent(filesControl, "Files");
            filesControl.LoadFiles();
        }

        public void ShowUsers()
        {
            HighlightNavButton(btnUsers);
            ShowContent(usersControl, "Users");
            usersControl.LoadUsers();
        }

        public void ShowKeys()
        {
            HighlightNavButton(btnKeys);
            ShowContent(keysControl, "Access Keys");
            keysControl.LoadKeys();
        }

        public void ShowPolicies()
        {
            HighlightNavButton(btnPolicies);
            ShowContent(policiesControl, "Policies");
            policiesControl.LoadPolicies();
        }

        public void ShowMonitoring()
        {
            HighlightNavButton(btnMonitoring);
            ShowContent(monitoringControl, "Monitoring");
            monitoringControl.LoadMetrics();
        }

        public void ShowSettings()
        {
            HighlightNavButton(btnSettings);
            ShowContent(settingsControl, "Settings");
        }

        private void BtnLogout_Click(object? sender, EventArgs e)
        {
            var result = MessageBox.Show("Are you sure you want to sign out?", "Sign Out",
                MessageBoxButtons.YesNo, MessageBoxIcon.Question);
            if (result == DialogResult.Yes)
            {
                Program.AuthService.Logout();
                var loginForm = new LoginForm();
                loginForm.Show();
                this.Close();
            }
        }
    }
}