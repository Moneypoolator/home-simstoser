using System;
using System.Drawing;
using System.Windows.Forms;
using S3StorageClient.Services;

namespace S3StorageClient.Forms
{
    public partial class LoginForm : Form
    {
        private TextBox txtServerUrl;
        private TextBox txtUsername;
        private TextBox txtPassword;
        private Button btnLogin;
        private Label lblError;
        private Panel mainPanel;

        public LoginForm()
        {
            InitializeComponent();
            this.StartPosition = FormStartPosition.CenterScreen;
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Text = "S3 Storage - Login";
            this.ClientSize = new Size(420, 520);
            this.BackColor = Color.FromArgb(248, 250, 252);
        }

        private void InitializeComponent()
        {
            mainPanel = new Panel
            {
                Size = new Size(380, 460),
                Location = new Point(20, 30),
                BackColor = Color.White,
                BorderStyle = BorderStyle.None
            };

            var titleLabel = new Label
            {
                Text = "S3 Storage",
                Font = new Font("Segoe UI", 24, FontStyle.Bold),
                ForeColor = Color.FromArgb(37, 99, 235),
                TextAlign = ContentAlignment.MiddleCenter,
                Size = new Size(380, 50),
                Location = new Point(0, 20)
            };

            var subtitleLabel = new Label
            {
                Text = "Sign in to access the storage",
                Font = new Font("Segoe UI", 10, FontStyle.Regular),
                ForeColor = Color.FromArgb(107, 114, 128),
                TextAlign = ContentAlignment.MiddleCenter,
                Size = new Size(380, 25),
                Location = new Point(0, 70)
            };

            // Server URL
            var lblServerUrl = new Label
            {
                Text = "Server URL",
                Font = new Font("Segoe UI", 9, FontStyle.Bold),
                ForeColor = Color.FromArgb(55, 65, 81),
                Location = new Point(30, 110),
                Size = new Size(320, 20)
            };

            txtServerUrl = new TextBox
            {
                Text = Program.ApiService.GetBaseUrl(),
                Font = new Font("Segoe UI", 10),
                Location = new Point(30, 132),
                Size = new Size(320, 30),
                BorderStyle = BorderStyle.FixedSingle
            };

            // Username
            var lblUsername = new Label
            {
                Text = "Username",
                Font = new Font("Segoe UI", 9, FontStyle.Bold),
                ForeColor = Color.FromArgb(55, 65, 81),
                Location = new Point(30, 175),
                Size = new Size(320, 20)
            };

            txtUsername = new TextBox
            {
                Font = new Font("Segoe UI", 10),
                Location = new Point(30, 197),
                Size = new Size(320, 30),
                BorderStyle = BorderStyle.FixedSingle
            };

            // Password
            var lblPassword = new Label
            {
                Text = "Password",
                Font = new Font("Segoe UI", 9, FontStyle.Bold),
                ForeColor = Color.FromArgb(55, 65, 81),
                Location = new Point(30, 240),
                Size = new Size(320, 20)
            };

            txtPassword = new TextBox
            {
                Font = new Font("Segoe UI", 10),
                Location = new Point(30, 262),
                Size = new Size(320, 30),
                BorderStyle = BorderStyle.FixedSingle,
                PasswordChar = '*',
                UseSystemPasswordChar = true
            };

            // Error label
            lblError = new Label
            {
                ForeColor = Color.FromArgb(220, 38, 38),
                Font = new Font("Segoe UI", 9, FontStyle.Regular),
                TextAlign = ContentAlignment.MiddleCenter,
                Size = new Size(320, 40),
                Location = new Point(30, 300),
                Visible = false
            };

            // Login button
            btnLogin = new Button
            {
                Text = "Sign In",
                Font = new Font("Segoe UI", 11, FontStyle.Bold),
                ForeColor = Color.White,
                BackColor = Color.FromArgb(37, 99, 235),
                FlatStyle = FlatStyle.Flat,
                FlatAppearance = { BorderSize = 0 },
                Size = new Size(320, 42),
                Location = new Point(30, 345),
                Cursor = Cursors.Hand
            };
            btnLogin.Click += BtnLogin_Click;

            // Test credentials info
            var testInfo = new Label
            {
                Text = "Test credentials:\nusername: admin\npassword: admin123",
                Font = new Font("Segoe UI", 9, FontStyle.Regular),
                ForeColor = Color.FromArgb(156, 163, 175),
                TextAlign = ContentAlignment.MiddleCenter,
                Size = new Size(320, 50),
                Location = new Point(30, 400)
            };

            mainPanel.Controls.AddRange(new Control[] {
                titleLabel, subtitleLabel,
                lblServerUrl, txtServerUrl,
                lblUsername, txtUsername,
                lblPassword, txtPassword,
                lblError, btnLogin, testInfo
            });

            this.Controls.Add(mainPanel);

            txtUsername.KeyPress += (s, e) => { if (e.KeyChar == (char)Keys.Enter) BtnLogin_Click(s, e); };
            txtPassword.KeyPress += (s, e) => { if (e.KeyChar == (char)Keys.Enter) BtnLogin_Click(s, e); };
            txtServerUrl.KeyPress += (s, e) => { if (e.KeyChar == (char)Keys.Enter) BtnLogin_Click(s, e); };
        }

        private async void BtnLogin_Click(object? sender, EventArgs e)
        {
            var serverUrl = txtServerUrl.Text.Trim();
            var username = txtUsername.Text.Trim();
            var password = txtPassword.Text;

            if (string.IsNullOrEmpty(serverUrl))
            {
                ShowError("Please enter the server URL");
                return;
            }

            if (string.IsNullOrEmpty(username) || string.IsNullOrEmpty(password))
            {
                ShowError("Please fill in all fields");
                return;
            }

            btnLogin.Enabled = false;
            btnLogin.Text = "Signing in...";
            lblError.Visible = false;

            try
            {
                Program.ApiService.SetBaseUrl(serverUrl);
                Properties.Settings.Default.ServerUrl = serverUrl;
                Properties.Settings.Default.Save();

                await Program.AuthService.LoginAsync(username, password);

                var mainForm = new MainForm();
                mainForm.Show();
                this.Hide();
            }
            catch (UnauthorizedAccessException)
            {
                ShowError("Invalid credentials. Please check your username and password.");
            }
            catch (ApiException ex)
            {
                ShowError($"Server error: {ex.Message}");
            }
            catch (Exception ex)
            {
                ShowError($"Connection error: {ex.Message}");
            }
            finally
            {
                btnLogin.Enabled = true;
                btnLogin.Text = "Sign In";
            }
        }

        private void ShowError(string message)
        {
            lblError.Text = message;
            lblError.Visible = true;
        }
    }
}