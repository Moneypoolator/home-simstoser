using System;
using S3StorageClient.Models;

namespace S3StorageClient.Services
{
    public class AuthService
    {
        private readonly ApiService _apiService;

        public AuthService(ApiService apiService)
        {
            _apiService = apiService;
        }

        public string? AccessKeyId { get; private set; }
        public string? SecretAccessKey { get; private set; }
        public string? UserId { get; private set; }
        public string? Username { get; private set; }

        public bool IsAuthenticated => !string.IsNullOrEmpty(AccessKeyId);

        public async Task<LoginResponse> LoginAsync(string username, string password)
        {
            var response = await _apiService.LoginAsync(username, password);

            AccessKeyId = response.AccessKeyId;
            SecretAccessKey = response.SecretAccessKey;
            UserId = response.UserId;
            Username = response.Username;

            _apiService.SetCredentials(AccessKeyId, SecretAccessKey);

            SaveToSettings();

            return response;
        }

        public void Logout()
        {
            AccessKeyId = null;
            SecretAccessKey = null;
            UserId = null;
            Username = null;

            _apiService.ClearCredentials();

            ClearSettings();
        }

        public bool TryRestoreSession()
        {
            var accessKeyId = Properties.Settings.Default.AccessKeyId;
            var secretAccessKey = Properties.Settings.Default.SecretAccessKey;
            var userId = Properties.Settings.Default.UserId;
            var username = Properties.Settings.Default.Username;

            if (!string.IsNullOrEmpty(accessKeyId) && !string.IsNullOrEmpty(secretAccessKey))
            {
                AccessKeyId = accessKeyId;
                SecretAccessKey = secretAccessKey;
                UserId = userId;
                Username = username;

                _apiService.SetCredentials(AccessKeyId, SecretAccessKey);
                return true;
            }

            return false;
        }

        private void SaveToSettings()
        {
            Properties.Settings.Default.AccessKeyId = AccessKeyId ?? string.Empty;
            Properties.Settings.Default.SecretAccessKey = SecretAccessKey ?? string.Empty;
            Properties.Settings.Default.UserId = UserId ?? string.Empty;
            Properties.Settings.Default.Username = Username ?? string.Empty;
            Properties.Settings.Default.Save();
        }

        private void ClearSettings()
        {
            Properties.Settings.Default.AccessKeyId = string.Empty;
            Properties.Settings.Default.SecretAccessKey = string.Empty;
            Properties.Settings.Default.UserId = string.Empty;
            Properties.Settings.Default.Username = string.Empty;
            Properties.Settings.Default.Save();
        }
    }
}