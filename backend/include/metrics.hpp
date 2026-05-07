#pragma once

#include <string>
#include <map>
#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>
#include <sstream>
#include <iomanip>
#include <glog/logging.h>

namespace metrics {

// Simple metrics collector for Prometheus exposition
class MetricsCollector {
public:
    static MetricsCollector& instance() {
        static MetricsCollector instance;
        return instance;
    }

    // Record a request with method, endpoint, and status code
    void record_request(const std::string& method, const std::string& endpoint, int status_code,
                        const std::chrono::milliseconds& duration_ms) {
        VLOG(3) << "Recording metrics for " << method << " " << endpoint << " status " << status_code;
        // Increment total requests
        total_requests_.fetch_add(1, std::memory_order_relaxed);
        
        // Increment request counter for this method
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        request_counts_[method]++;
        
        // Record status code
        if (status_code >= 400 && status_code < 500) {
            client_errors_.fetch_add(1, std::memory_order_relaxed);
        } else if (status_code >= 500) {
            server_errors_.fetch_add(1, std::memory_order_relaxed);
        }
        
        // Record latency
        record_latency(duration_ms);
        
        // Record endpoint-specific metrics
        std::string key = method + ":" + endpoint;
        endpoint_counts_[key]++;
        VLOG(3) << "Metrics recorded";
    }

    // Record an error by type
    void record_error(const std::string& error_type) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        error_counts_[error_type]++;
    }

    // Record latency in milliseconds
    void record_latency(const std::chrono::milliseconds& duration_ms) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        // Simple histogram buckets: <10ms, <50ms, <100ms, <500ms, <1000ms, <5000ms, >=5000ms
        if (duration_ms.count() < 10) latency_buckets_[0]++;
        else if (duration_ms.count() < 50) latency_buckets_[1]++;
        else if (duration_ms.count() < 100) latency_buckets_[2]++;
        else if (duration_ms.count() < 500) latency_buckets_[3]++;
        else if (duration_ms.count() < 1000) latency_buckets_[4]++;
        else if (duration_ms.count() < 5000) latency_buckets_[5]++;
        else latency_buckets_[6]++;
        
        // Update total latency for average calculation
        total_latency_ms_ += duration_ms.count();
        latency_samples_++;
    }

    // Generate Prometheus metrics format
    std::string to_prometheus() const {
        std::ostringstream out;
        out << std::fixed << std::setprecision(2);
        
        // HELP and TYPE comments (optional but good practice)
        out << "# HELP s3_server_requests_total Total number of HTTP requests\n";
        out << "# TYPE s3_server_requests_total counter\n";
        out << "s3_server_requests_total " << total_requests_.load(std::memory_order_relaxed) << "\n";
        
        out << "# HELP s3_server_requests_by_method_total Total requests by HTTP method\n";
        out << "# TYPE s3_server_requests_by_method_total counter\n";
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            for (const auto& [method, count] : request_counts_) {
                out << "s3_server_requests_by_method_total{method=\"" << method << "\"} " << count << "\n";
            }
        }
        
        out << "# HELP s3_server_client_errors_total Total client errors (4xx)\n";
        out << "# TYPE s3_server_client_errors_total counter\n";
        out << "s3_server_client_errors_total " << client_errors_.load(std::memory_order_relaxed) << "\n";
        
        out << "# HELP s3_server_server_errors_total Total server errors (5xx)\n";
        out << "# TYPE s3_server_server_errors_total counter\n";
        out << "s3_server_server_errors_total " << server_errors_.load(std::memory_order_relaxed) << "\n";
        
        out << "# HELP s3_server_errors_by_type_total Total errors by error type\n";
        out << "# TYPE s3_server_errors_by_type_total counter\n";
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            for (const auto& [error_type, count] : error_counts_) {
                out << "s3_server_errors_by_type_total{error_type=\"" << error_type << "\"} " << count << "\n";
            }
        }
        
        out << "# HELP s3_server_request_duration_seconds Histogram of request durations in seconds\n";
        out << "# TYPE s3_server_request_duration_seconds histogram\n";
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            const std::vector<double> bucket_upper_bounds = {0.01, 0.05, 0.1, 0.5, 1.0, 5.0};
            uint64_t cumulative = 0;
            for (size_t i = 0; i < bucket_upper_bounds.size(); ++i) {
                cumulative += latency_buckets_[i];
                out << "s3_server_request_duration_seconds_bucket{le=\"" << bucket_upper_bounds[i] << "\"} " << cumulative << "\n";
            }
            cumulative += latency_buckets_[6]; // +Inf bucket
            out << "s3_server_request_duration_seconds_bucket{le=\"+Inf\"} " << cumulative << "\n";
            
            // Add sum and count
            out << "s3_server_request_duration_seconds_sum " 
                << (total_latency_ms_.load() / 1000.0) << "\n";
            out << "s3_server_request_duration_seconds_count " << latency_samples_.load() << "\n";
        }
        
        out << "# HELP s3_server_active_sessions Current active sessions\n";
        out << "# TYPE s3_server_active_sessions gauge\n";
        out << "s3_server_active_sessions " << active_sessions_.load(std::memory_order_relaxed) << "\n";
        
        return out.str();
    }

    // Update active sessions count
    void set_active_sessions(int count) {
        active_sessions_.store(count, std::memory_order_relaxed);
    }

    // Increment/decrement active sessions
    void increment_active_sessions() {
        active_sessions_.fetch_add(1, std::memory_order_relaxed);
    }
    
    void decrement_active_sessions() {
        active_sessions_.fetch_sub(1, std::memory_order_relaxed);
    }

private:
    MetricsCollector() 
        : total_requests_(0)
        , client_errors_(0)
        , server_errors_(0)
        , active_sessions_(0)
        , total_latency_ms_(0)
        , latency_samples_(0) {
        latency_buckets_.resize(7, 0); // 7 buckets
    }
    
    ~MetricsCollector() = default;
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;

    std::atomic<uint64_t> total_requests_;
    std::atomic<uint64_t> client_errors_;
    std::atomic<uint64_t> server_errors_;
    std::atomic<int> active_sessions_;
    
    mutable std::recursive_mutex mutex_;
    std::map<std::string, uint64_t> request_counts_;
    std::map<std::string, uint64_t> error_counts_;
    std::map<std::string, uint64_t> endpoint_counts_;
    
    // Latency histogram buckets
    std::vector<uint64_t> latency_buckets_;
    std::atomic<uint64_t> total_latency_ms_;
    std::atomic<uint64_t> latency_samples_;
};

// Helper class for measuring request duration
class ScopedTimer {
public:
    ScopedTimer(const std::string& method, const std::string& endpoint)
        : method_(method)
        , endpoint_(endpoint)
        , start_(std::chrono::steady_clock::now()) {}
    
    ~ScopedTimer() {
        // Destructor doesn't need to do anything
        // Duration recording is done via elapsed() and explicit record_request call
    }
    
    std::chrono::milliseconds elapsed() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_);
    }
    
    const std::string& method() const { return method_; }
    const std::string& endpoint() const { return endpoint_; }
    
private:
    std::string method_;
    std::string endpoint_;
    std::chrono::steady_clock::time_point start_;
};

} // namespace metrics