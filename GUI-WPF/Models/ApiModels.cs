using System;
using Newtonsoft.Json;

namespace S3StorageClient.Models
{
    public class FileMetadata
    {
        [JsonProperty("name")]
        public string Name { get; set; } = string.Empty;

        [JsonProperty("size")]
        public long Size { get; set; }

        [JsonProperty("last_modified")]
        public string LastModified { get; set; } = string.Empty;

        [JsonProperty("etag")]
        public string Etag { get; set; } = string.Empty;
    }

    public class FileListResponse
    {
        [JsonProperty("count")]
        public int Count { get; set; }

        [JsonProperty("files")]
        public List<FileMetadata> Files { get; set; } = new();
    }

    public class FileUploadResponse
    {
        [JsonProperty("success")]
        public bool Success { get; set; }

        [JsonProperty("filename")]
        public string Filename { get; set; } = string.Empty;

        [JsonProperty("size")]
        public long Size { get; set; }

        [JsonProperty("etag")]
        public string Etag { get; set; } = string.Empty;
    }

    public class FileDeleteResponse
    {
        [JsonProperty("success")]
        public bool Success { get; set; }

        [JsonProperty("message")]
        public string Message { get; set; } = string.Empty;
    }

    public class AccessKey
    {
        [JsonProperty("access_key_id")]
        public string AccessKeyId { get; set; } = string.Empty;

        [JsonProperty("secret_access_key")]
        public string? SecretAccessKey { get; set; }

        [JsonProperty("user_name")]
        public string UserName { get; set; } = string.Empty;

        [JsonProperty("is_active")]
        public bool IsActive { get; set; }

        [JsonProperty("created_at")]
        public string CreatedAt { get; set; } = string.Empty;
    }

    public class AccessKeyListResponse
    {
        [JsonProperty("keys")]
        public List<AccessKey> Keys { get; set; } = new();
    }

    public class User
    {
        [JsonProperty("user_id")]
        public string UserId { get; set; } = string.Empty;

        [JsonProperty("username")]
        public string Username { get; set; } = string.Empty;

        [JsonProperty("email")]
        public string Email { get; set; } = string.Empty;

        [JsonProperty("role")]
        public string Role { get; set; } = "VIEWER";

        [JsonProperty("is_active")]
        public bool IsActive { get; set; }

        [JsonProperty("created_at")]
        public string CreatedAt { get; set; } = string.Empty;

        [JsonProperty("last_login")]
        public string? LastLogin { get; set; }
    }

    public class UserListResponse
    {
        [JsonProperty("users")]
        public List<User> Users { get; set; } = new();
    }

    public class Permission
    {
        [JsonProperty("type")]
        public string Type { get; set; } = "READ";

        [JsonProperty("resource_pattern")]
        public string ResourcePattern { get; set; } = "*";

        [JsonProperty("allow")]
        public bool Allow { get; set; } = true;
    }

    public class AccessPolicy
    {
        [JsonProperty("policy_id")]
        public string PolicyId { get; set; } = string.Empty;

        [JsonProperty("name")]
        public string Name { get; set; } = string.Empty;

        [JsonProperty("description")]
        public string Description { get; set; } = string.Empty;

        [JsonProperty("permissions")]
        public List<Permission> Permissions { get; set; } = new();

        [JsonProperty("created_at")]
        public string CreatedAt { get; set; } = string.Empty;
    }

    public class PolicyListResponse
    {
        [JsonProperty("policies")]
        public List<AccessPolicy> Policies { get; set; } = new();
    }

    public class LoginResponse
    {
        [JsonProperty("access_key_id")]
        public string AccessKeyId { get; set; } = string.Empty;

        [JsonProperty("secret_access_key")]
        public string SecretAccessKey { get; set; } = string.Empty;

        [JsonProperty("user_id")]
        public string UserId { get; set; } = string.Empty;

        [JsonProperty("username")]
        public string Username { get; set; } = string.Empty;
    }

    public class MultipartInitiateResponse
    {
        [JsonProperty("upload_id")]
        public string UploadId { get; set; } = string.Empty;

        [JsonProperty("filename")]
        public string Filename { get; set; } = string.Empty;

        [JsonProperty("message")]
        public string Message { get; set; } = string.Empty;
    }

    public class MultipartPartUploadResponse
    {
        [JsonProperty("success")]
        public bool Success { get; set; }

        [JsonProperty("upload_id")]
        public string UploadId { get; set; } = string.Empty;

        [JsonProperty("part_number")]
        public int PartNumber { get; set; }

        [JsonProperty("size")]
        public long Size { get; set; }
    }

    public class MultipartCompleteResponse
    {
        [JsonProperty("success")]
        public bool Success { get; set; }

        [JsonProperty("upload_id")]
        public string UploadId { get; set; } = string.Empty;

        [JsonProperty("message")]
        public string Message { get; set; } = string.Empty;
    }

    public class MultipartAbortResponse
    {
        [JsonProperty("success")]
        public bool Success { get; set; }

        [JsonProperty("upload_id")]
        public string UploadId { get; set; } = string.Empty;

        [JsonProperty("message")]
        public string Message { get; set; } = string.Empty;
    }

    public class MultipartProgressResponse
    {
        [JsonProperty("upload_id")]
        public string UploadId { get; set; } = string.Empty;

        [JsonProperty("parts")]
        public Dictionary<string, long> Parts { get; set; } = new();
    }

    public class ServerMetrics
    {
        [JsonProperty("total_requests")]
        public long TotalRequests { get; set; }

        [JsonProperty("client_errors")]
        public long ClientErrors { get; set; }

        [JsonProperty("server_errors")]
        public long ServerErrors { get; set; }

        [JsonProperty("request_counts")]
        public Dictionary<string, long> RequestCounts { get; set; } = new();

        [JsonProperty("endpoint_counts")]
        public Dictionary<string, long> EndpointCounts { get; set; } = new();

        [JsonProperty("latency_percentiles")]
        public LatencyPercentiles LatencyPercentiles { get; set; } = new();

        [JsonProperty("system_info")]
        public SystemInfo SystemInfo { get; set; } = new();

        [JsonProperty("rate_limiting")]
        public RateLimitingInfo RateLimiting { get; set; } = new();
    }

    public class LatencyPercentiles
    {
        [JsonProperty("p50")]
        public double P50 { get; set; }

        [JsonProperty("p90")]
        public double P90 { get; set; }

        [JsonProperty("p95")]
        public double P95 { get; set; }

        [JsonProperty("p99")]
        public double P99 { get; set; }
    }

    public class SystemInfo
    {
        [JsonProperty("uptime")]
        public double Uptime { get; set; }

        [JsonProperty("memory_usage")]
        public long MemoryUsage { get; set; }

        [JsonProperty("active_connections")]
        public int ActiveConnections { get; set; }
    }

    public class RateLimitingInfo
    {
        [JsonProperty("active_bans")]
        public int ActiveBans { get; set; }

        [JsonProperty("total_banned")]
        public int TotalBanned { get; set; }

        [JsonProperty("requests_per_minute")]
        public double RequestsPerMinute { get; set; }
    }

    public class SystemStatus
    {
        [JsonProperty("status")]
        public string Status { get; set; } = "unhealthy";

        [JsonProperty("message")]
        public string Message { get; set; } = string.Empty;

        [JsonProperty("timestamp")]
        public string Timestamp { get; set; } = string.Empty;
    }

    public class ErrorResponse
    {
        [JsonProperty("error")]
        public string Error { get; set; } = string.Empty;

        [JsonProperty("message")]
        public string? Message { get; set; }
    }

    public class ApiResponse<T>
    {
        [JsonProperty("success")]
        public bool Success { get; set; }

        [JsonProperty("data")]
        public T? Data { get; set; }

        [JsonProperty("error")]
        public string? Error { get; set; }

        [JsonProperty("message")]
        public string? Message { get; set; }
    }
}