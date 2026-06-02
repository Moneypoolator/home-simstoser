using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace S3StorageClient.Views
{
    public partial class MainWindow : Window
    {
        private readonly DashboardPage dashboardPage;
        private readonly FilesPage filesPage;
        private readonly UsersPage usersPage;
        private readonly KeysPage keysPage;
        private readonly PoliciesPage policiesPage;
        private readonly MonitoringPage monitoringPage;
        private readonly SettingsPage settingsPage;

        public MainWindow()
        {
            InitializeComponent();

            lblUsername.Text = App.AuthService.Username ?? "";

            dashboardPage = new DashboardPage();
            filesPage = new FilesPage();
            usersPage = new UsersPage();
            keysPage = new KeysPage();
            policiesPage = new PoliciesPage();
            monitoringPage = new MonitoringPage();
            settingsPage = new SettingsPage();

            ShowDashboard();
        }

        private void HighlightNavButton(Button activeBtn)
        {
            var buttons = new[] { btnDashboard, btnFiles, btnUsers, btnKeys, btnPolicies, btnMonitoring, btnSettings };
            foreach (var btn in buttons)
            {
                btn.Background = Brushes.Transparent;
                btn.FontWeight = FontWeights.Normal;
            }
            activeBtn.Background = new SolidColorBrush(Color.FromRgb(37, 99, 235));
            activeBtn.FontWeight = FontWeights.Bold;
        }

        private void ShowContent(UIElement control, string title)
        {
            contentArea.Content = control;
            lblTitle.Text = title;
        }

        public void ShowDashboard()
        {
            HighlightNavButton(btnDashboard);
            ShowContent(dashboardPage, "Dashboard");
            dashboardPage.LoadData();
        }

        public void ShowFiles()
        {
            HighlightNavButton(btnFiles);
            ShowContent(filesPage, "Files");
            filesPage.LoadFiles();
        }

        public void ShowUsers()
        {
            HighlightNavButton(btnUsers);
            ShowContent(usersPage, "Users");
            usersPage.LoadUsers();
        }

        public void ShowKeys()
        {
            HighlightNavButton(btnKeys);
            ShowContent(keysPage, "Access Keys");
            keysPage.LoadKeys();
        }

        public void ShowPolicies()
        {
            HighlightNavButton(btnPolicies);
            ShowContent(policiesPage, "Policies");
            policiesPage.LoadPolicies();
        }

        public void ShowMonitoring()
        {
            HighlightNavButton(btnMonitoring);
            ShowContent(monitoringPage, "Monitoring");
            monitoringPage.LoadMetrics();
        }

        public void ShowSettings()
        {
            HighlightNavButton(btnSettings);
            ShowContent(settingsPage, "Settings");
        }

        private void BtnDashboard_Click(object sender, RoutedEventArgs e) => ShowDashboard();
        private void BtnFiles_Click(object sender, RoutedEventArgs e) => ShowFiles();
        private void BtnUsers_Click(object sender, RoutedEventArgs e) => ShowUsers();
        private void BtnKeys_Click(object sender, RoutedEventArgs e) => ShowKeys();
        private void BtnPolicies_Click(object sender, RoutedEventArgs e) => ShowPolicies();
        private void BtnMonitoring_Click(object sender, RoutedEventArgs e) => ShowMonitoring();
        private void BtnSettings_Click(object sender, RoutedEventArgs e) => ShowSettings();

        private void BtnLogout_Click(object sender, RoutedEventArgs e)
        {
            var result = MessageBox.Show("Are you sure you want to sign out?", "Sign Out",
                MessageBoxButton.YesNo, MessageBoxImage.Question);
            if (result == MessageBoxResult.Yes)
            {
                App.AuthService.Logout();
                var loginWindow = new LoginWindow();
                loginWindow.Show();
                this.Close();
            }
        }
    }
}