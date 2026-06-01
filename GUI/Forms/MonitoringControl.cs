using System;
using System.Drawing;
using System.Windows.Forms;
using S3StorageClient.Models;

namespace S3StorageClient.Forms
{
    public class MonitoringControl : UserControl
    {
        private System.Windows.Forms.Timer refreshTimer;
        private CheckBox chkAutoRefresh;
        private Button btnRefresh;
        private Label lblStatus;

        // Status card
        private Panel statusCard;
        private Label lblStatusIcon;
        private Label lblStatusText;
        private Label lblStatusMessage;
        private Label lblLastCheck;

        // Uptime card
        private Label lblUptimeValue;

        // Connections card
        private Label lblConnectionsValue;

        // Request stats
        private Label lblTotalRequests;
        private Label lblClientErrors;
        private Label lblServerErrors;
        private Label lblSuccessRequests;

        // Latency stats
        private Label lblP50;
        private Label lblP90;
        private Label lblP95;
        private Label lblP99;

        // Rate limiting
        private Label lblActiveBans;
        private Label lblTotalBanned;
        private Label lblRequestsPerMinute;

        // Memory usage
        private Label lblMemoryValue;
        private ProgressBar memoryBar;

        // Request methods
        private FlowLayoutPanel methodsPanel;

        private ServerMetrics? _metrics;
        private SystemStatus? _systemStatus;

        public MonitoringControl()
        {
            InitializeComponent();
            refreshTimer = new System.Windows.Forms.Timer { Interval = 10000 };
            refreshTimer.Tick += (s, e) => { if (chkAutoRefresh.Checked) LoadMetrics(); };
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
                Text = "Monitoring",
                Font = new Font("Segoe UI", 20, FontStyle.Bold),
                ForeColor = Color.FromArgb(17, 24, 39),
                Location = new Point(10, 10),
                AutoSize = true
            };

            var lblSubtitle = new Label
            {
                Text = "Server status and performance metrics",
                Font = new Font("Segoe UI", 10, FontStyle.Regular),
                ForeColor = Color.FromArgb(107, 114, 128),
                Location = new Point(10, 40),
                AutoSize = true
            };

            chkAutoRefresh = new CheckBox
            {
                Text = "Auto-refresh (10s)",
                Location = new Point(400, 15),
                AutoSize = true,
                Checked = true
            };
            chkAutoRefresh.CheckedChanged += (s, e) =>
            {
                if (chkAutoRefresh.Checked) refreshTimer.Start();
                else refreshTimer.Stop();
            };

            btnRefresh = new Button
            {
                Text = "Refresh",
                Font = new Font("Segoe UI", 9, FontStyle.Regular),
                BackColor = Color.FromArgb(37, 99, 235),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                FlatAppearance = { BorderSize = 0 },
                Location = new Point(530, 12),
                Size = new Size(90, 30),
                Cursor = Cursors.Hand
            };
            btnRefresh.Click += (s, e) => LoadMetrics();

            headerPanel.Controls.AddRange(new Control[] { lblTitle, lblSubtitle, chkAutoRefresh, btnRefresh });

            // ===== Content panel (scrollable) =====
            var contentPanel = new FlowLayoutPanel
            {
                Dock = DockStyle.Fill,
                FlowDirection = FlowDirection.TopDown,
                WrapContents = false,
                AutoScroll = true,
                Padding = new Padding(0, 10, 0, 0)
            };

            // ---- Row 1: Status cards ----
            var row1 = new FlowLayoutPanel
            {
                Size = new Size(800, 160),
                FlowDirection = FlowDirection.LeftToRight,
                WrapContents = false,
                Padding = new Padding(0)
            };

            statusCard = CreateMetricCard("Server Status", "Current system state", 250, 140);
            lblStatusIcon = new Label
            {
                Font = new Font("Segoe UI", 24, FontStyle.Regular),
                Location = new Point(210, 10),
                AutoSize = true
            };
            lblStatusText = new Label
            {
                Font = new Font("Segoe UI", 10, FontStyle.Bold),
                Location = new Point(10, 50),
                AutoSize = true
            };
            lblStatusMessage = new Label
            {
                Font = new Font("Segoe UI", 9, FontStyle.Regular),
                ForeColor = Color.FromArgb(75, 85, 99),
                Location = new Point(10, 70),
                AutoSize = true
            };
            lblLastCheck = new Label
            {
                Font = new Font("Segoe UI", 8, FontStyle.Italic),
                ForeColor = Color.FromArgb(156, 163, 175),
                Location = new Point(10, 90),
                AutoSize = true
            };
            statusCard.Controls.AddRange(new Control[] { lblStatusIcon, lblStatusText, lblStatusMessage, lblLastCheck });

            var uptimeCard = CreateMetricCard("Uptime", "Since server start", 250, 140);
            lblUptimeValue = new Label
            {
                Font = new Font("Segoe UI", 24, FontStyle.Bold),
                ForeColor = Color.FromArgb(17, 24, 39),
                Location = new Point(10, 60),
                AutoSize = true
            };
            uptimeCard.Controls.Add(lblUptimeValue);

            var connectionsCard = CreateMetricCard("Active Connections", "Current connections", 250, 140);
            lblConnectionsValue = new Label
            {
                Font = new Font("Segoe UI", 24, FontStyle.Bold),
                ForeColor = Color.FromArgb(17, 24, 39),
                Location = new Point(10, 60),
                AutoSize = true
            };
            connectionsCard.Controls.Add(lblConnectionsValue);

            row1.Controls.AddRange(new Control[] { statusCard, uptimeCard, connectionsCard });

            // ---- Row 2: Request stats + Latency ----
            var row2 = new FlowLayoutPanel
            {
                Size = new Size(800, 220),
                FlowDirection = FlowDirection.LeftToRight,
                WrapContents = false,
                Padding = new Padding(0)
            };

            var requestCard = CreateMetricCard("Request Statistics", "Total processed requests", 380, 200);
            lblTotalRequests = CreateStatLabel(10, 50, "Total Requests", "0");
            lblClientErrors = CreateStatLabel(10, 75, "Client Errors (4xx)", "0");
            lblServerErrors = CreateStatLabel(10, 100, "Server Errors (5xx)", "0");
            lblSuccessRequests = CreateStatLabel(10, 125, "Successful Requests", "0");
            requestCard.Controls.AddRange(new Control[] { lblTotalRequests, lblClientErrors, lblServerErrors, lblSuccessRequests });

            var latencyCard = CreateMetricCard("Latency (ms)", "Response time percentiles", 380, 200);
            lblP50 = CreateStatLabel(10, 50, "P50 (median)", "0.00 ms");
            lblP90 = CreateStatLabel(10, 75, "P90", "0.00 ms");
            lblP95 = CreateStatLabel(10, 100, "P95", "0.00 ms");
            lblP99 = CreateStatLabel(10, 125, "P99", "0.00 ms");
            latencyCard.Controls.AddRange(new Control[] { lblP50, lblP90, lblP95, lblP99 });

            row2.Controls.AddRange(new Control[] { requestCard, latencyCard });

            // ---- Row 3: Rate limiting + Memory ----
            var row3 = new FlowLayoutPanel
            {
                Size = new Size(800, 220),
                FlowDirection = FlowDirection.LeftToRight,
                WrapContents = false,
                Padding = new Padding(0)
            };

            var rateCard = CreateMetricCard("Rate Limiting", "DDoS protection and limits", 380, 200);
            lblActiveBans = CreateStatLabel(10, 50, "Active Bans", "0");
            lblTotalBanned = CreateStatLabel(10, 75, "Total Banned", "0");
            lblRequestsPerMinute = CreateStatLabel(10, 100, "Requests/Minute", "0.0");
            rateCard.Controls.AddRange(new Control[] { lblActiveBans, lblTotalBanned, lblRequestsPerMinute });

            var memoryCard = CreateMetricCard("Memory Usage", "Server memory consumption", 380, 200);
            lblMemoryValue = new Label
            {
                Font = new Font("Segoe UI", 10, FontStyle.Bold),
                ForeColor = Color.FromArgb(17, 24, 39),
                Location = new Point(10, 50),
                AutoSize = true
            };
            memoryBar = new ProgressBar
            {
                Location = new Point(10, 80),
                Size = new Size(340, 20),
                Style = ProgressBarStyle.Continuous,
                ForeColor = Color.FromArgb(79, 70, 229),
                BackColor = Color.FromArgb(229, 231, 235)
            };
            var lblMemoryHint = new Label
            {
                Font = new Font("Segoe UI", 8, FontStyle.Italic),
                ForeColor = Color.FromArgb(156, 163, 175),
                Location = new Point(10, 110),
                AutoSize = true
            };
            memoryCard.Controls.AddRange(new Control[] { lblMemoryValue, memoryBar, lblMemoryHint });

            row3.Controls.AddRange(new Control[] { rateCard, memoryCard });

            // ---- Row 4: Request Methods ----
            var methodsCard = new Panel
            {
                Size = new Size(780, 180),
                BackColor = Color.White,
                Padding = new Padding(10)
            };

            var lblMethodsTitle = new Label
            {
                Text = "Request Methods Distribution",
                Font = new Font("Segoe UI", 12, FontStyle.Bold),
                ForeColor = Color.FromArgb(17, 24, 39),
                Location = new Point(10, 10),
                AutoSize = true
            };
            var lblMethodsSubtitle = new Label
            {
                Text = "Request count by HTTP method",
                Font = new Font("Segoe UI", 9, FontStyle.Regular),
                ForeColor = Color.FromArgb(107, 114, 128),
                Location = new Point(10, 30),
                AutoSize = true
            };

            methodsPanel = new FlowLayoutPanel
            {
                Location = new Point(10, 55),
                Size = new Size(740, 110),
                FlowDirection = FlowDirection.LeftToRight,
                WrapContents = true
            };

            methodsCard.Controls.AddRange(new Control[] { lblMethodsTitle, lblMethodsSubtitle, methodsPanel });

            contentPanel.Controls.AddRange(new Control[] { row1, row2, row3, methodsCard });

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

        private Panel CreateMetricCard(string title, string subtitle, int width, int height)
        {
            var card = new Panel
            {
                Size = new Size(width, height),
                BackColor = Color.White,
                Margin = new Padding(0, 0, 10, 10)
            };

            var lblTitle = new Label
            {
                Text = title,
                Font = new Font("Segoe UI", 10, FontStyle.Bold),
                ForeColor = Color.FromArgb(17, 24, 39),
                Location = new Point(10, 10),
                AutoSize = true
            };

            var lblSubtitle = new Label
            {
                Text = subtitle,
                Font = new Font("Segoe UI", 8, FontStyle.Regular),
                ForeColor = Color.FromArgb(107, 114, 128),
                Location = new Point(10, 28),
                AutoSize = true
            };

            card.Controls.AddRange(new Control[] { lblTitle, lblSubtitle });
            return card;
        }

        private Label CreateStatLabel(int x, int y, string label, string value)
        {
            var lbl = new Label
            {
                Location = new Point(x, y),
                Size = new Size(350, 22),
                AutoSize = false
            };

            var lblLabel = new Label
            {
                Text = label,
                Font = new Font("Segoe UI", 9, FontStyle.Regular),
                ForeColor = Color.FromArgb(75, 85, 99),
                Location = new Point(0, 0),
                AutoSize = true
            };

            var lblValue = new Label
            {
                Text = value,
                Font = new Font("Segoe UI", 9, FontStyle.Bold),
                ForeColor = Color.FromArgb(17, 24, 39),
                Location = new Point(250, 0),
                AutoSize = true
            };

            lbl.Controls.AddRange(new Control[] { lblLabel, lblValue });
            return lbl;
        }

        public async void LoadMetrics()
        {
            try
            {
                lblStatus.Text = "Loading metrics...";

                // Load system status
                _systemStatus = await Program.ApiService.GetSystemStatusAsync();

                // Load raw metrics and parse them
                try
                {
                    var rawMetrics = await Program.ApiService.GetRawMetricsAsync();
                    _metrics = Program.ApiService.ParsePrometheusMetrics(rawMetrics);
                }
                catch
                {
                    // Metrics endpoint may not be available
                }

                UpdateUI();
                lblStatus.Text = "Metrics updated";
            }
            catch (Exception ex)
            {
                lblStatus.Text = $"Error: {ex.Message}";
            }
        }

        private void UpdateUI()
        {
            // System status
            if (_systemStatus != null)
            {
                switch (_systemStatus.Status)
                {
                    case "healthy":
                        lblStatusIcon.Text = "✓";
                        lblStatusIcon.ForeColor = Color.FromArgb(34, 197, 94);
                        lblStatusText.Text = "Running normally";
                        lblStatusText.ForeColor = Color.FromArgb(34, 197, 94);
                        break;
                    case "degraded":
                        lblStatusIcon.Text = "⚠";
                        lblStatusIcon.ForeColor = Color.FromArgb(234, 179, 8);
                        lblStatusText.Text = "Degraded performance";
                        lblStatusText.ForeColor = Color.FromArgb(234, 179, 8);
                        break;
                    default:
                        lblStatusIcon.Text = "✗";
                        lblStatusIcon.ForeColor = Color.FromArgb(239, 68, 68);
                        lblStatusText.Text = "Unavailable";
                        lblStatusText.ForeColor = Color.FromArgb(239, 68, 68);
                        break;
                }
                lblStatusMessage.Text = _systemStatus.Message;

                if (DateTime.TryParse(_systemStatus.Timestamp, out var ts))
                    lblLastCheck.Text = $"Last check: {ts.ToLocalTime():HH:mm:ss}";
            }

            if (_metrics == null) return;

            // Uptime
            var uptime = _metrics.SystemInfo.Uptime;
            if (uptime > 0)
            {
                var days = (int)(uptime / 86400);
                var hours = (int)((uptime % 86400) / 3600);
                var minutes = (int)((uptime % 3600) / 60);
                lblUptimeValue.Text = days > 0 ? $"{days}d {hours}h" : hours > 0 ? $"{hours}h {minutes}m" : $"{minutes}m";
            }
            else
            {
                lblUptimeValue.Text = "0m";
            }

            // Connections
            lblConnectionsValue.Text = _metrics.SystemInfo.ActiveConnections.ToString();

            // Request stats
            UpdateStatLabel(lblTotalRequests, "Total Requests", _metrics.TotalRequests.ToString("N0"));
            UpdateStatLabel(lblClientErrors, "Client Errors (4xx)", _metrics.ClientErrors.ToString("N0"));
            UpdateStatLabel(lblServerErrors, "Server Errors (5xx)", _metrics.ServerErrors.ToString("N0"));
            var success = _metrics.TotalRequests - _metrics.ClientErrors - _metrics.ServerErrors;
            UpdateStatLabel(lblSuccessRequests, "Successful Requests", Math.Max(0, success).ToString("N0"));

            // Latency
            UpdateStatLabel(lblP50, "P50 (median)", $"{_metrics.LatencyPercentiles.P50:F2} ms");
            UpdateStatLabel(lblP90, "P90", $"{_metrics.LatencyPercentiles.P90:F2} ms");
            UpdateStatLabel(lblP95, "P95", $"{_metrics.LatencyPercentiles.P95:F2} ms");
            UpdateStatLabel(lblP99, "P99", $"{_metrics.LatencyPercentiles.P99:F2} ms");

            // Rate limiting
            UpdateStatLabel(lblActiveBans, "Active Bans", _metrics.RateLimiting.ActiveBans.ToString());
            UpdateStatLabel(lblTotalBanned, "Total Banned", _metrics.RateLimiting.TotalBanned.ToString());
            UpdateStatLabel(lblRequestsPerMinute, "Requests/Minute", _metrics.RateLimiting.RequestsPerMinute.ToString("F1"));

            // Memory
            var memMB = _metrics.SystemInfo.MemoryUsage / (1024.0 * 1024.0);
            lblMemoryValue.Text = $"{FormatBytes(_metrics.SystemInfo.MemoryUsage)} ({memMB:F0} MB)";
            memoryBar.Value = Math.Min(100, (int)(memMB / 100.0 * 100));
            if (memoryBar.Parent?.Controls.Count > 2 && memoryBar.Parent.Controls[2] is Label hint)
            {
                hint.Text = $"Approximately {memMB:F0} MB of ~100 MB";
            }

            // Request methods
            methodsPanel.Controls.Clear();
            if (_metrics.RequestCounts.Count > 0)
            {
                foreach (var kvp in _metrics.RequestCounts)
                {
                    var methodCard = new Panel
                    {
                        Size = new Size(120, 70),
                        BackColor = Color.FromArgb(249, 250, 251),
                        Margin = new Padding(0, 0, 10, 10)
                    };

                    var lblMethod = new Label
                    {
                        Text = kvp.Key,
                        Font = new Font("Segoe UI", 9, FontStyle.Bold),
                        ForeColor = Color.FromArgb(107, 114, 128),
                        Location = new Point(10, 10),
                        AutoSize = true
                    };

                    var lblCount = new Label
                    {
                        Text = kvp.Value.ToString("N0"),
                        Font = new Font("Segoe UI", 18, FontStyle.Bold),
                        ForeColor = Color.FromArgb(17, 24, 39),
                        Location = new Point(10, 30),
                        AutoSize = true
                    };

                    methodCard.Controls.AddRange(new Control[] { lblMethod, lblCount });
                    methodsPanel.Controls.Add(methodCard);
                }
            }
            else
            {
                var lblNoData = new Label
                {
                    Text = "No request method data available",
                    Font = new Font("Segoe UI", 9, FontStyle.Italic),
                    ForeColor = Color.FromArgb(156, 163, 175),
                    Location = new Point(10, 10),
                    AutoSize = true
                };
                methodsPanel.Controls.Add(lblNoData);
            }
        }

        private void UpdateStatLabel(Label container, string label, string value)
        {
            if (container.Controls.Count >= 2)
            {
                container.Controls[0].Text = label;
                container.Controls[1].Text = value;
            }
        }

        private static string FormatBytes(long bytes)
        {
            if (bytes < 1024) return $"{bytes} B";
            if (bytes < 1024 * 1024) return $"{bytes / 1024.0:F1} KB";
            if (bytes < 1024 * 1024 * 1024) return $"{bytes / (1024.0 * 1024.0):F1} MB";
            return $"{bytes / (1024.0 * 1024.0 * 1024.0):F2} GB";
        }

        protected override void OnLoad(EventArgs e)
        {
            base.OnLoad(e);
            LoadMetrics();
            if (chkAutoRefresh.Checked)
                refreshTimer.Start();
        }
    }
}