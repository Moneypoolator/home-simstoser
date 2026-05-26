import api from './api';

export interface ServerMetrics {
  total_requests: number;
  client_errors: number;
  server_errors: number;
  request_counts: Record<string, number>;
  endpoint_counts: Record<string, number>;
  latency_percentiles: {
    p50: number;
    p90: number;
    p95: number;
    p99: number;
  };
  system_info: {
    uptime: number;
    memory_usage: number;
    active_connections: number;
  };
  rate_limiting: {
    active_bans: number;
    total_banned: number;
    requests_per_minute: number;
  };
}

export interface PrometheusMetric {
  name: string;
  help: string;
  type: string;
  metrics: Array<{
    labels: Record<string, string>;
    value: number;
    timestamp?: number;
  }>;
}

export const metricsService = {
  async getMetrics(): Promise<string> {
    const response = await api.get('/metrics', {
      headers: {
        'Accept': 'text/plain',
      },
    });
    return response.data;
  },

  async getMetricsJson(): Promise<ServerMetrics> {
    const metricsText = await this.getMetrics();
    return this.parsePrometheusMetrics(metricsText);
  },

  parsePrometheusMetrics(metricsText: string): ServerMetrics {
    const lines = metricsText.split('\n');
    const metrics: ServerMetrics = {
      total_requests: 0,
      client_errors: 0,
      server_errors: 0,
      request_counts: {},
      endpoint_counts: {},
      latency_percentiles: {
        p50: 0,
        p90: 0,
        p95: 0,
        p99: 0,
      },
      system_info: {
        uptime: 0,
        memory_usage: 0,
        active_connections: 0,
      },
      rate_limiting: {
        active_bans: 0,
        total_banned: 0,
        requests_per_minute: 0,
      },
    };

    for (const line of lines) {
      if (line.startsWith('#') || line.trim() === '') continue;

      const match = line.match(/^(\w+)(?:\{([^}]+)\})?\s+([\d\.]+)/);
      if (!match) continue;

      const [, name, labelsStr, valueStr] = match;
      const value = parseFloat(valueStr);

      switch (name) {
        case 's3_server_requests_total':
          metrics.total_requests = value;
          break;
        case 's3_server_client_errors_total':
          metrics.client_errors = value;
          break;
        case 's3_server_server_errors_total':
          metrics.server_errors = value;
          break;
        case 's3_server_request_count':
          if (labelsStr) {
            const methodMatch = labelsStr.match(/method="([^"]+)"/);
            if (methodMatch) {
              metrics.request_counts[methodMatch[1]] = value;
            }
          }
          break;
        case 's3_server_endpoint_count':
          if (labelsStr) {
            const endpointMatch = labelsStr.match(/endpoint="([^"]+)"/);
            if (endpointMatch) {
              metrics.endpoint_counts[endpointMatch[1]] = value;
            }
          }
          break;
        case 's3_server_latency_percentile':
          if (labelsStr) {
            const percentileMatch = labelsStr.match(/percentile="([^"]+)"/);
            if (percentileMatch) {
              const percentile = percentileMatch[1];
              if (percentile === '0.5') metrics.latency_percentiles.p50 = value;
              if (percentile === '0.9') metrics.latency_percentiles.p90 = value;
              if (percentile === '0.95') metrics.latency_percentiles.p95 = value;
              if (percentile === '0.99') metrics.latency_percentiles.p99 = value;
            }
          }
          break;
        case 's3_server_uptime_seconds':
          metrics.system_info.uptime = value;
          break;
        case 's3_server_memory_usage_bytes':
          metrics.system_info.memory_usage = value;
          break;
        case 's3_server_active_connections':
          metrics.system_info.active_connections = value;
          break;
        case 's3_server_rate_limit_active_bans':
          metrics.rate_limiting.active_bans = value;
          break;
        case 's3_server_rate_limit_total_banned':
          metrics.rate_limiting.total_banned = value;
          break;
        case 's3_server_requests_per_minute':
          metrics.rate_limiting.requests_per_minute = value;
          break;
      }
    }

    return metrics;
  },

  async getSystemStatus(): Promise<{
    status: 'healthy' | 'degraded' | 'unhealthy';
    message: string;
    timestamp: string;
  }> {
    try {
      await api.get('/list');
      return {
        status: 'healthy',
        message: 'Сервер работает нормально',
        timestamp: new Date().toISOString(),
      };
    } catch (error: any) {
      if (error.response?.status === 401 || error.response?.status === 403) {
        // Authentication error means server is running but auth failed
        return {
          status: 'healthy',
          message: 'Сервер работает (ошибка аутентификации)',
          timestamp: new Date().toISOString(),
        };
      } else if (error.response?.status === 429) {
        // Rate limit means server is running
        return {
          status: 'degraded',
          message: 'Сервер работает (превышен лимит запросов)',
          timestamp: new Date().toISOString(),
        };
      } else {
        return {
          status: 'unhealthy',
          message: 'Сервер недоступен',
          timestamp: new Date().toISOString(),
        };
      }
    }
  },
};