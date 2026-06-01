using System;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;
using S3StorageClient.Models;

namespace S3StorageClient.Services
{
    public class ApiService
    {
        private readonly HttpClient _httpClient;
        private string _baseUrl = "http://localhost:9000";
        private string? _accessKeyId;
        private string? _secretAccessKey;

        public ApiService()
        {
            _httpClient = new HttpClient();
            _httpClient.DefaultRequestHeaders.Accept.Add(
                new MediaTypeWithQualityHeaderValue("application/json"));
        }

        public void SetBaseUrl(string baseUrl)
        {
            _baseUrl = baseUrl.TrimEnd('/');
        }

        public string GetBaseUrl() => _baseUrl;

        public void SetCredentials(string accessKeyId, string secretAccessKey)
        {
            _accessKeyId = accessKeyId;
            _secretAccessKey = secretAccessKey;
        }

        public void ClearCredentials()
        {
            _accessKeyId = null;
            _secretAccessKey = null;
        }

        public bool HasCredentials() => !string.IsNullOrEmpty(_accessKeyId);

        private void AddAuthHeaders(HttpRequestMessage request)
        {
            if (!string.IsNullOrEmpty(_accessKeyId))
            {
                request.Headers.Add("X-Access-Key", _accessKeyId);
            }
        }

        private async Task<T> SendAsync<T>(HttpRequestMessage request)
        {
            AddAuthHeaders(request);

            try
            {
                var response = await _httpClient.SendAsync(request);
                var content = await response.Content.ReadAsStringAsync();

                if (!response.IsSuccessStatusCode)
                {
                    string errorMsg;
                    try
                    {
                        var errorResponse = JsonConvert.DeserializeObject<ErrorResponse>(content);
                        errorMsg = errorResponse?.Error ?? errorResponse?.Message ?? response.ReasonPhrase ?? "Unknown error";
                    }
                    catch
                    {
                        errorMsg = $"HTTP {(int)response.StatusCode}: {response.ReasonPhrase}";
                    }

                    if (response.StatusCode == System.Net.HttpStatusCode.Unauthorized)
                    {
                        throw new UnauthorizedAccessException(errorMsg);
                    }

                    throw new ApiException(errorMsg, (int)response.StatusCode);
                }

                if (typeof(T) == typeof(byte[]))
                {
                    var bytes = await response.Content.ReadAsByteArrayAsync();
                    return (T)(object)bytes;
                }

                if (typeof(T) == typeof(string))
                {
                    return (T)(object)content;
                }

                return JsonConvert.DeserializeObject<T>(content) ?? throw new ApiException("Failed to deserialize response", 0);
            }
            catch (HttpRequestException ex)
            {
                throw new ApiException($"Connection error: {ex.Message}", 0);
            }
        }

        // ========== Auth ==========
        public async Task<LoginResponse> LoginAsync(string username, string password)
        {
            var payload = new { username, password };
            var json = JsonConvert.SerializeObject(payload);
            var request = new HttpRequestMessage(HttpMethod.Post, $"{_baseUrl}/auth/login")
            {
                Content = new StringContent(json, Encoding.UTF8, "application/json")
            };

            return await SendAsync<LoginResponse>(request);
        }

        // ========== Files ==========
        public async Task<FileListResponse> ListFilesAsync()
        {
            var request = new HttpRequestMessage(HttpMethod.Get, $"{_baseUrl}/list");
            return await SendAsync<FileListResponse>(request);
        }

        public async Task<byte[]> DownloadFileAsync(string filename)
        {
            var request = new HttpRequestMessage(HttpMethod.Get, $"{_baseUrl}/{Uri.EscapeDataString(filename)}");
            request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue("application/octet-stream"));
            return await SendAsync<byte[]>(request);
        }

        public async Task<FileUploadResponse> UploadFileAsync(string filename, byte[] data, string contentType = "application/octet-stream")
        {
            var request = new HttpRequestMessage(HttpMethod.Put, $"{_baseUrl}/{Uri.EscapeDataString(filename)}")
            {
                Content = new ByteArrayContent(data)
            };
            request.Content.Headers.ContentType = new MediaTypeHeaderValue(contentType);
            return await SendAsync<FileUploadResponse>(request);
        }

        public async Task<FileDeleteResponse> DeleteFileAsync(string filename)
        {
            var request = new HttpRequestMessage(HttpMethod.Delete, $"{_baseUrl}/{Uri.EscapeDataString(filename)}");
            return await SendAsync<FileDeleteResponse>(request);
        }

        // ========== Multipart Upload ==========
        public async Task<MultipartInitiateResponse> InitiateMultipartUploadAsync(string filename)
        {
            var request = new HttpRequestMessage(HttpMethod.Post,
                $"{_baseUrl}/upload/initiate?filename={Uri.EscapeDataString(filename)}");
            return await SendAsync<MultipartInitiateResponse>(request);
        }

        public async Task<MultipartPartUploadResponse> UploadPartAsync(string uploadId, int partNumber, byte[] data)
        {
            var request = new HttpRequestMessage(HttpMethod.Put,
                $"{_baseUrl}/upload/part?upload_id={Uri.EscapeDataString(uploadId)}&part_number={partNumber}")
            {
                Content = new ByteArrayContent(data)
            };
            request.Content.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
            return await SendAsync<MultipartPartUploadResponse>(request);
        }

        public async Task<MultipartCompleteResponse> CompleteMultipartUploadAsync(string uploadId, List<int> parts)
        {
            var payload = new { parts };
            var json = JsonConvert.SerializeObject(payload);
            var request = new HttpRequestMessage(HttpMethod.Post,
                $"{_baseUrl}/upload/complete?upload_id={Uri.EscapeDataString(uploadId)}")
            {
                Content = new StringContent(json, Encoding.UTF8, "application/json")
            };
            return await SendAsync<MultipartCompleteResponse>(request);
        }

        public async Task<MultipartAbortResponse> AbortMultipartUploadAsync(string uploadId)
        {
            var request = new HttpRequestMessage(HttpMethod.Delete,
                $"{_baseUrl}/upload/abort?upload_id={Uri.EscapeDataString(uploadId)}");
            return await SendAsync<MultipartAbortResponse>(request);
        }

        public async Task<MultipartProgressResponse> GetUploadProgressAsync(string uploadId)
        {
            var request = new HttpRequestMessage(HttpMethod.Get,
                $"{_baseUrl}/upload/progress?upload_id={Uri.EscapeDataString(uploadId)}");
            return await SendAsync<MultipartProgressResponse>(request);
        }

        // ========== Admin: Keys ==========
        public async Task<AccessKeyListResponse> ListKeysAsync()
        {
            var request = new HttpRequestMessage(HttpMethod.Get, $"{_baseUrl}/admin/keys");
            return await SendAsync<AccessKeyListResponse>(request);
        }

        public async Task<AccessKey> CreateKeyAsync(string username)
        {
            var payload = new { username };
            var json = JsonConvert.SerializeObject(payload);
            var request = new HttpRequestMessage(HttpMethod.Post, $"{_baseUrl}/admin/keys")
            {
                Content = new StringContent(json, Encoding.UTF8, "application/json")
            };
            return await SendAsync<AccessKey>(request);
        }

        public async Task DeleteKeyAsync(string accessKeyId)
        {
            var request = new HttpRequestMessage(HttpMethod.Delete, $"{_baseUrl}/admin/keys/{Uri.EscapeDataString(accessKeyId)}");
            await SendAsync<object>(request);
        }

        public async Task<AccessKey> ActivateKeyAsync(string accessKeyId)
        {
            var request = new HttpRequestMessage(HttpMethod.Post, $"{_baseUrl}/admin/keys/{Uri.EscapeDataString(accessKeyId)}/activate");
            return await SendAsync<AccessKey>(request);
        }

        public async Task<AccessKey> DeactivateKeyAsync(string accessKeyId)
        {
            var request = new HttpRequestMessage(HttpMethod.Post, $"{_baseUrl}/admin/keys/{Uri.EscapeDataString(accessKeyId)}/deactivate");
            return await SendAsync<AccessKey>(request);
        }

        // ========== Admin: Users ==========
        public async Task<UserListResponse> ListUsersAsync()
        {
            var request = new HttpRequestMessage(HttpMethod.Get, $"{_baseUrl}/admin/users");
            return await SendAsync<UserListResponse>(request);
        }

        public async Task<User> CreateUserAsync(string username, string email, string role = "VIEWER")
        {
            var payload = new { username, email, role };
            var json = JsonConvert.SerializeObject(payload);
            var request = new HttpRequestMessage(HttpMethod.Post, $"{_baseUrl}/admin/users")
            {
                Content = new StringContent(json, Encoding.UTF8, "application/json")
            };
            return await SendAsync<User>(request);
        }

        public async Task<User> UpdateUserRoleAsync(string userId, string role)
        {
            var payload = new { role };
            var json = JsonConvert.SerializeObject(payload);
            var request = new HttpRequestMessage(HttpMethod.Put, $"{_baseUrl}/admin/users/{Uri.EscapeDataString(userId)}/role")
            {
                Content = new StringContent(json, Encoding.UTF8, "application/json")
            };
            return await SendAsync<User>(request);
        }

        public async Task<User> ActivateUserAsync(string userId)
        {
            var request = new HttpRequestMessage(HttpMethod.Post, $"{_baseUrl}/admin/users/{Uri.EscapeDataString(userId)}/activate");
            return await SendAsync<User>(request);
        }

        public async Task<User> DeactivateUserAsync(string userId)
        {
            var request = new HttpRequestMessage(HttpMethod.Post, $"{_baseUrl}/admin/users/{Uri.EscapeDataString(userId)}/deactivate");
            return await SendAsync<User>(request);
        }

        public async Task DeleteUserAsync(string userId)
        {
            var request = new HttpRequestMessage(HttpMethod.Delete, $"{_baseUrl}/admin/users/{Uri.EscapeDataString(userId)}");
            await SendAsync<object>(request);
        }

        // ========== Admin: Policies ==========
        public async Task<PolicyListResponse> ListPoliciesAsync()
        {
            var request = new HttpRequestMessage(HttpMethod.Get, $"{_baseUrl}/admin/policies");
            return await SendAsync<PolicyListResponse>(request);
        }

        public async Task<AccessPolicy> CreatePolicyAsync(string name, string description, List<Permission> permissions)
        {
            var payload = new { name, description, permissions };
            var json = JsonConvert.SerializeObject(payload);
            var request = new HttpRequestMessage(HttpMethod.Post, $"{_baseUrl}/admin/policies")
            {
                Content = new StringContent(json, Encoding.UTF8, "application/json")
            };
            return await SendAsync<AccessPolicy>(request);
        }

        public async Task<AccessPolicy> GetPolicyAsync(string policyId)
        {
            var request = new HttpRequestMessage(HttpMethod.Get, $"{_baseUrl}/admin/policies/{Uri.EscapeDataString(policyId)}");
            return await SendAsync<AccessPolicy>(request);
        }

        public async Task<AccessPolicy> UpdatePolicyAsync(string policyId, string name, string description, List<Permission> permissions)
        {
            var payload = new { name, description, permissions };
            var json = JsonConvert.SerializeObject(payload);
            var request = new HttpRequestMessage(HttpMethod.Put, $"{_baseUrl}/admin/policies/{Uri.EscapeDataString(policyId)}")
            {
                Content = new StringContent(json, Encoding.UTF8, "application/json")
            };
            return await SendAsync<AccessPolicy>(request);
        }

        public async Task DeletePolicyAsync(string policyId)
        {
            var request = new HttpRequestMessage(HttpMethod.Delete, $"{_baseUrl}/admin/policies/{Uri.EscapeDataString(policyId)}");
            await SendAsync<object>(request);
        }

        public async Task AttachPolicyToUserAsync(string userId, string policyId)
        {
            var payload = new { policy_id = policyId };
            var json = JsonConvert.SerializeObject(payload);
            var request = new HttpRequestMessage(HttpMethod.Post, $"{_baseUrl}/admin/users/{Uri.EscapeDataString(userId)}/policies")
            {
                Content = new StringContent(json, Encoding.UTF8, "application/json")
            };
            await SendAsync<object>(request);
        }

        public async Task DetachPolicyFromUserAsync(string userId, string policyId)
        {
            var request = new HttpRequestMessage(HttpMethod.Delete,
                $"{_baseUrl}/admin/users/{Uri.EscapeDataString(userId)}/policies/{Uri.EscapeDataString(policyId)}");
            await SendAsync<object>(request);
        }

        public async Task<PolicyListResponse> ListUserPoliciesAsync(string userId)
        {
            var request = new HttpRequestMessage(HttpMethod.Get, $"{_baseUrl}/admin/users/{Uri.EscapeDataString(userId)}/policies");
            return await SendAsync<PolicyListResponse>(request);
        }

        // ========== Metrics ==========
        public async Task<string> GetRawMetricsAsync()
        {
            var request = new HttpRequestMessage(HttpMethod.Get, $"{_baseUrl}/metrics");
            request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue("text/plain"));
            return await SendAsync<string>(request);
        }

        public async Task<SystemStatus> GetSystemStatusAsync()
        {
            try
            {
                await ListFilesAsync();
                return new SystemStatus
                {
                    Status = "healthy",
                    Message = "Server is running normally",
                    Timestamp = DateTime.UtcNow.ToString("o")
                };
            }
            catch (UnauthorizedAccessException)
            {
                return new SystemStatus
                {
                    Status = "healthy",
                    Message = "Server is running (authentication error)",
                    Timestamp = DateTime.UtcNow.ToString("o")
                };
            }
            catch (ApiException ex) when (ex.StatusCode == 429)
            {
                return new SystemStatus
                {
                    Status = "degraded",
                    Message = "Server is running (rate limit exceeded)",
                    Timestamp = DateTime.UtcNow.ToString("o")
                };
            }
            catch
            {
                return new SystemStatus
                {
                    Status = "unhealthy",
                    Message = "Server is unavailable",
                    Timestamp = DateTime.UtcNow.ToString("o")
                };
            }
        }

        public ServerMetrics ParsePrometheusMetrics(string metricsText)
        {
            var metrics = new ServerMetrics();
            var lines = metricsText.Split('\n');

            foreach (var line in lines)
            {
                if (line.StartsWith("#") || string.IsNullOrWhiteSpace(line))
                    continue;

                var match = System.Text.RegularExpressions.Regex.Match(line,
                    @"^(\w+)(?:\{([^}]+)\})?\s+([\d\.]+)");
                if (!match.Success) continue;

                var name = match.Groups[1].Value;
                var labelsStr = match.Groups[2].Value;
                var value = double.Parse(match.Groups[3].Value,
                    System.Globalization.CultureInfo.InvariantCulture);

                switch (name)
                {
                    case "s3_server_requests_total":
                        metrics.TotalRequests = (long)value;
                        break;
                    case "s3_server_client_errors_total":
                        metrics.ClientErrors = (long)value;
                        break;
                    case "s3_server_server_errors_total":
                        metrics.ServerErrors = (long)value;
                        break;
                    case "s3_server_request_count":
                        var methodMatch = System.Text.RegularExpressions.Regex.Match(labelsStr, @"method=""([^""]+)""");
                        if (methodMatch.Success)
                            metrics.RequestCounts[methodMatch.Groups[1].Value] = (long)value;
                        break;
                    case "s3_server_endpoint_count":
                        var endpointMatch = System.Text.RegularExpressions.Regex.Match(labelsStr, @"endpoint=""([^""]+)""");
                        if (endpointMatch.Success)
                            metrics.EndpointCounts[endpointMatch.Groups[1].Value] = (long)value;
                        break;
                    case "s3_server_latency_percentile":
                        var percentileMatch = System.Text.RegularExpressions.Regex.Match(labelsStr, @"percentile=""([^""]+)""");
                        if (percentileMatch.Success)
                        {
                            var p = percentileMatch.Groups[1].Value;
                            if (p == "0.5") metrics.LatencyPercentiles.P50 = value;
                            if (p == "0.9") metrics.LatencyPercentiles.P90 = value;
                            if (p == "0.95") metrics.LatencyPercentiles.P95 = value;
                            if (p == "0.99") metrics.LatencyPercentiles.P99 = value;
                        }
                        break;
                    case "s3_server_uptime_seconds":
                        metrics.SystemInfo.Uptime = value;
                        break;
                    case "s3_server_memory_usage_bytes":
                        metrics.SystemInfo.MemoryUsage = (long)value;
                        break;
                    case "s3_server_active_connections":
                        metrics.SystemInfo.ActiveConnections = (int)value;
                        break;
                    case "s3_server_rate_limit_active_bans":
                        metrics.RateLimiting.ActiveBans = (int)value;
                        break;
                    case "s3_server_rate_limit_total_banned":
                        metrics.RateLimiting.TotalBanned = (int)value;
                        break;
                    case "s3_server_requests_per_minute":
                        metrics.RateLimiting.RequestsPerMinute = value;
                        break;
                }
            }

            return metrics;
        }
    }

    public class ApiException : Exception
    {
        public int StatusCode { get; }

        public ApiException(string message, int statusCode) : base(message)
        {
            StatusCode = statusCode;
        }
    }
}