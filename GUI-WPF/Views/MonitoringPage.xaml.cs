using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Threading;
using S3StorageClient.Models;

namespace S3StorageClient.Views
{
    public partial class MonitoringPage : UserControl
    {
        private readonly DispatcherTimer _refreshTimer;
        private ServerMetrics _metrics;
        private SystemStatus _systemStatus;

        public MonitoringPage()
        {
            InitializeComponent();

            _refreshTimer = new DispatcherTimer
            {
                Interval = TimeSpan.FromSeconds(10)
            };
            _refreshTimer.Tick += (s, e) =>
            {
                if (chkAutoRefresh.IsChecked == true)
                    LoadMetrics();
            };
        }

        public async void LoadMetrics()
        {
            try
            {
                lblStatus.Text = "Loading metrics...";

                // Load system status
                _systemStatus = await App.ApiService.GetSystemStatusAsync();

                // Load raw metrics and parse them
                try
                {
                    var rawMetrics = await App.ApiService.GetRawMetricsAsync();
                    _metrics = App.ApiService.ParsePrometheusMetrics(rawMetrics);
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
                        lblStatusIcon.Text = "\u2713";
                        lblStatusIcon.Foreground = new SolidColorBrush(Color.FromRgb(34, 197, 94));
                        lblStatusText.Text = "Running normally";
                        lblStatusText.Foreground = new SolidColorBrush(Color.FromRgb(34, 197, 94));
                        break;
                    case "degraded":
                        lblStatusIcon.Text = "\u26A0";
                        lblStatusIcon.Foreground = new SolidColorBrush(Color.FromRgb(234, 179, 8));
                        lblStatusText.Text = "Degraded performance";
                        lblStatusText.Foreground = new SolidColorBrush(Color.FromRgb(234, 179, 8));
                        break;
                    default:
                        lblStatusIcon.Text = "\u2717";
                        lblStatusIcon.Foreground = new SolidColorBrush(Color.FromRgb(239, 68, 68));
                        lblStatusText.Text = "Unavailable";
                        lblStatusText.Foreground = new SolidColorBrush(Color.FromRgb(239, 68, 68));
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
            lblTotalRequests.Text = _metrics.TotalRequests.ToString("N0");
            lblClientErrors.Text = _metrics.ClientErrors.ToString("N0");
            lblServerErrors.Text = _metrics.ServerErrors.ToString("N0");
            var success = _metrics.TotalRequests - _metrics.ClientErrors - _metrics.ServerErrors;
            lblSuccessRequests.Text = Math.Max(0, success).ToString("N0");

            // Latency
            lblP50.Text = $"{_metrics.LatencyPercentiles.P50:F2} ms";
            lblP90.Text = $"{_metrics.LatencyPercentiles.P90:F2} ms";
            lblP95.Text = $"{_metrics.LatencyPercentiles.P95:F2} ms";
            lblP99.Text = $"{_metrics.LatencyPercentiles.P99:F2} ms";

            // Rate limiting
            lblActiveBans.Text = _metrics.RateLimiting.ActiveBans.ToString();
            lblTotalBanned.Text = _metrics.RateLimiting.TotalBanned.ToString();
            lblRequestsPerMinute.Text = _metrics.RateLimiting.RequestsPerMinute.ToString("F1");

            // Memory
            var memMB = _metrics.SystemInfo.MemoryUsage / (1024.0 * 1024.0);
            lblMemoryValue.Text = $"{FormatBytes(_metrics.SystemInfo.MemoryUsage)} ({memMB:F0} MB)";
            memoryBar.Value = Math.Min(100, (int)(memMB / 100.0 * 100));

            // Request methods
            methodsPanel.Children.Clear();
            if (_metrics.RequestCounts.Count > 0)
            {
                foreach (var kvp in _metrics.RequestCounts)
                {
                    var methodCard = new Border
                    {
                        Width = 120,
                        Height = 70,
                        Background = new SolidColorBrush(Color.FromRgb(249, 250, 251)),
                        Margin = new Thickness(0, 0, 10, 10),
                        CornerRadius = new CornerRadius(4),
                        Child = new Grid
                        {
                            Margin = new Thickness(10),
                            RowDefinitions =
                            {
                                new RowDefinition { Height = GridLength.Auto },
                                new RowDefinition { Height = GridLength.Auto }
                            },
                            Children =
                            {
                                new TextBlock
                                {
                                    Text = kvp.Key,
                                    FontSize = 9,
                                    FontWeight = FontWeights.Bold,
                                    Foreground = new SolidColorBrush(Color.FromRgb(107, 114, 128))
                                },
                                new TextBlock
                                {
                                    Text = kvp.Value.ToString("N0"),
                                    FontSize = 18,
                                    FontWeight = FontWeights.Bold,
                                    Foreground = new SolidColorBrush(Color.FromRgb(17, 24, 39)),
                                    Margin = new Thickness(0, 5, 0, 0)
                                }
                            }
                        }
                    };
                    // Set grid row for the count textblock
                    var grid = (Grid)methodCard.Child;
                    Grid.SetRow((TextBlock)grid.Children[1], 1);

                    methodsPanel.Children.Add(methodCard);
                }
            }
            else
            {
                methodsPanel.Children.Add(new TextBlock
                {
                    Text = "No request method data available",
                    FontSize = 9,
                    FontStyle = FontStyles.Italic,
                    Foreground = new SolidColorBrush(Color.FromRgb(156, 163, 175))
                });
            }
        }

        private void BtnRefresh_Click(object sender, RoutedEventArgs e) => LoadMetrics();

        private void ChkAutoRefresh_Checked(object sender, RoutedEventArgs e)
        {
            _refreshTimer.Start();
        }

        private void ChkAutoRefresh_Unchecked(object sender, RoutedEventArgs e)
        {
            _refreshTimer.Stop();
        }

        private static string FormatBytes(long bytes)
        {
            if (bytes < 1024) return $"{bytes} B";
            if (bytes < 1024 * 1024) return $"{bytes / 1024.0:F1} KB";
            if (bytes < 1024 * 1024 * 1024) return $"{bytes / (1024.0 * 1024.0):F1} MB";
            return $"{bytes / (1024.0 * 1024.0 * 1024.0):F2} GB";
        }
    }
}