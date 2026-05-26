import React, { useEffect, useState } from 'react';
import { Header } from '../components/layout/Header';
import { Sidebar } from '../components/layout/Sidebar';
import { LoadingSpinner } from '../components/common/LoadingSpinner';
import { metricsService, ServerMetrics } from '../services/metrics';
import { Activity, Server, AlertTriangle, CheckCircle, XCircle, Clock, Users, BarChart3, Cpu, Shield, Database, Network } from 'lucide-react';
import { formatBytes } from '../utils/format';

export const MonitoringPage: React.FC = () => {
  const [metrics, setMetrics] = useState<ServerMetrics | null>(null);
  const [systemStatus, setSystemStatus] = useState<{
    status: 'healthy' | 'degraded' | 'unhealthy';
    message: string;
    timestamp: string;
  } | null>(null);
  const [loading, setLoading] = useState(true);
  const [autoRefresh, setAutoRefresh] = useState(true);

  const loadMetrics = async () => {
    try {
      setLoading(true);
      const [metricsData, statusData] = await Promise.all([
        metricsService.getMetricsJson(),
        metricsService.getSystemStatus(),
      ]);
      setMetrics(metricsData);
      setSystemStatus(statusData);
    } catch (error) {
      console.error('Error loading metrics:', error);
      setSystemStatus({
        status: 'unhealthy',
        message: 'Не удалось загрузить метрики',
        timestamp: new Date().toISOString(),
      });
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadMetrics();

    if (autoRefresh) {
      const interval = setInterval(loadMetrics, 10000); // Refresh every 10 seconds
      return () => clearInterval(interval);
    }
  }, [autoRefresh]);

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'healthy': return 'bg-green-100 text-green-800';
      case 'degraded': return 'bg-yellow-100 text-yellow-800';
      case 'unhealthy': return 'bg-red-100 text-red-800';
      default: return 'bg-gray-100 text-gray-800';
    }
  };

  const getStatusIcon = (status: string) => {
    switch (status) {
      case 'healthy': return <CheckCircle className="w-5 h-5 text-green-500" />;
      case 'degraded': return <AlertTriangle className="w-5 h-5 text-yellow-500" />;
      case 'unhealthy': return <XCircle className="w-5 h-5 text-red-500" />;
      default: return <Activity className="w-5 h-5 text-gray-500" />;
    }
  };

  const formatUptime = (seconds: number) => {
    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    
    if (days > 0) return `${days}д ${hours}ч`;
    if (hours > 0) return `${hours}ч ${minutes}м`;
    return `${minutes}м`;
  };

  return (
    <div className="flex h-screen overflow-hidden bg-gray-50">
      <Sidebar />
      <div className="flex-1 flex flex-col overflow-hidden">
        <Header />
        
        <main className="flex-1 overflow-y-auto p-6">
          <div className="max-w-7xl mx-auto">
            {/* Header */}
            <div className="flex items-center justify-between mb-8">
              <div>
                <h1 className="text-3xl font-bold text-gray-900">Мониторинг</h1>
                <p className="text-gray-500 mt-2">Статус сервера и метрики производительности</p>
              </div>
              <div className="flex items-center space-x-4">
                <div className="flex items-center">
                  <input
                    type="checkbox"
                    id="auto-refresh"
                    checked={autoRefresh}
                    onChange={(e) => setAutoRefresh(e.target.checked)}
                    className="h-4 w-4 text-primary-600 focus:ring-primary-500 border-gray-300 rounded"
                  />
                  <label htmlFor="auto-refresh" className="ml-2 text-sm text-gray-700">
                    Автообновление (10 сек)
                  </label>
                </div>
                <button
                  onClick={loadMetrics}
                  disabled={loading}
                  className="inline-flex items-center px-4 py-2 border border-transparent rounded-md shadow-sm text-sm font-medium text-white bg-primary-600 hover:bg-primary-700 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-primary-500 disabled:opacity-50"
                >
                  <Refresh className="w-4 h-4 mr-2" />
                  Обновить
                </button>
              </div>
            </div>

            {loading && !metrics ? (
              <div className="flex justify-center items-center h-64">
                <LoadingSpinner size="lg" />
              </div>
            ) : (
              <>
                {/* System Status Card */}
                <div className="grid grid-cols-1 lg:grid-cols-3 gap-6 mb-8">
                  <div className="bg-white rounded-lg shadow p-6">
                    <div className="flex items-center justify-between mb-4">
                      <div className="flex items-center">
                        <div className="p-3 rounded-lg bg-primary-100">
                          <Server className="w-6 h-6 text-primary-600" />
                        </div>
                        <div className="ml-4">
                          <h3 className="text-lg font-semibold text-gray-900">Статус сервера</h3>
                          <p className="text-sm text-gray-500">Текущее состояние системы</p>
                        </div>
                      </div>
                      {systemStatus && getStatusIcon(systemStatus.status)}
                    </div>
                    
                    {systemStatus && (
                      <div className="space-y-4">
                        <div className={`inline-flex items-center px-3 py-1 rounded-full text-sm font-medium ${getStatusColor(systemStatus.status)}`}>
                          {systemStatus.status === 'healthy' && 'Работает нормально'}
                          {systemStatus.status === 'degraded' && 'Снижена производительность'}
                          {systemStatus.status === 'unhealthy' && 'Недоступен'}
                        </div>
                        <p className="text-gray-700">{systemStatus.message}</p>
                        <p className="text-sm text-gray-500">
                          Последняя проверка: {new Date(systemStatus.timestamp).toLocaleTimeString()}
                        </p>
                      </div>
                    )}
                  </div>

                  {/* Uptime Card */}
                  <div className="bg-white rounded-lg shadow p-6">
                    <div className="flex items-center mb-4">
                      <div className="p-3 rounded-lg bg-blue-100">
                        <Clock className="w-6 h-6 text-blue-600" />
                      </div>
                      <div className="ml-4">
                        <h3 className="text-lg font-semibold text-gray-900">Время работы</h3>
                        <p className="text-sm text-gray-500">С момента запуска сервера</p>
                      </div>
                    </div>
                    <div className="text-3xl font-bold text-gray-900">
                      {metrics?.system_info.uptime ? formatUptime(metrics.system_info.uptime) : '0м'}
                    </div>
                  </div>

                  {/* Active Connections Card */}
                  <div className="bg-white rounded-lg shadow p-6">
                    <div className="flex items-center mb-4">
                      <div className="p-3 rounded-lg bg-green-100">
                        <Users className="w-6 h-6 text-green-600" />
                      </div>
                      <div className="ml-4">
                        <h3 className="text-lg font-semibold text-gray-900">Активные соединения</h3>
                        <p className="text-sm text-gray-500">Текущие подключения к серверу</p>
                      </div>
                    </div>
                    <div className="text-3xl font-bold text-gray-900">
                      {metrics?.system_info.active_connections || 0}
                    </div>
                  </div>
                </div>

                {/* Metrics Grid */}
                <div className="grid grid-cols-1 lg:grid-cols-2 gap-6 mb-8">
                  {/* Request Statistics */}
                  <div className="bg-white rounded-lg shadow p-6">
                    <div className="flex items-center mb-6">
                      <div className="p-3 rounded-lg bg-purple-100">
                        <BarChart3 className="w-6 h-6 text-purple-600" />
                      </div>
                      <div className="ml-4">
                        <h3 className="text-lg font-semibold text-gray-900">Статистика запросов</h3>
                        <p className="text-sm text-gray-500">Всего обработанных запросов</p>
                      </div>
                    </div>
                    
                    <div className="space-y-4">
                      <div className="flex justify-between items-center">
                        <span className="text-gray-600">Всего запросов</span>
                        <span className="font-semibold">{metrics?.total_requests || 0}</span>
                      </div>
                      <div className="flex justify-between items-center">
                        <span className="text-gray-600">Ошибки клиента (4xx)</span>
                        <span className="font-semibold text-yellow-600">{metrics?.client_errors || 0}</span>
                      </div>
                      <div className="flex justify-between items-center">
                        <span className="text-gray-600">Ошибки сервера (5xx)</span>
                        <span className="font-semibold text-red-600">{metrics?.server_errors || 0}</span>
                      </div>
                      <div className="flex justify-between items-center">
                        <span className="text-gray-600">Успешные запросы</span>
                        <span className="font-semibold text-green-600">
                          {(metrics?.total_requests || 0) - (metrics?.client_errors || 0) - (metrics?.server_errors || 0)}
                        </span>
                      </div>
                    </div>
                  </div>

                  {/* Latency Statistics */}
                  <div className="bg-white rounded-lg shadow p-6">
                    <div className="flex items-center mb-6">
                      <div className="p-3 rounded-lg bg-orange-100">
                        <Cpu className="w-6 h-6 text-orange-600" />
                      </div>
                      <div className="ml-4">
                        <h3 className="text-lg font-semibold text-gray-900">Задержки (мс)</h3>
                        <p className="text-sm text-gray-500">Процентили времени ответа</p>
                      </div>
                    </div>
                    
                    <div className="space-y-4">
                      <div className="flex justify-between items-center">
                        <span className="text-gray-600">P50 (медиана)</span>
                        <span className="font-semibold">{metrics?.latency_percentiles.p50?.toFixed(2) || '0.00'} мс</span>
                      </div>
                      <div className="flex justify-between items-center">
                        <span className="text-gray-600">P90</span>
                        <span className="font-semibold">{metrics?.latency_percentiles.p90?.toFixed(2) || '0.00'} мс</span>
                      </div>
                      <div className="flex justify-between items-center">
                        <span className="text-gray-600">P95</span>
                        <span className="font-semibold">{metrics?.latency_percentiles.p95?.toFixed(2) || '0.00'} мс</span>
                      </div>
                      <div className="flex justify-between items-center">
                        <span className="text-gray-600">P99</span>
                        <span className="font-semibold">{metrics?.latency_percentiles.p99?.toFixed(2) || '0.00'} мс</span>
                      </div>
                    </div>
                  </div>
                </div>

                {/* Rate Limiting & Memory Usage */}
                <div className="grid grid-cols-1 lg:grid-cols-2 gap-6 mb-8">
                  {/* Rate Limiting */}
                  <div className="bg-white rounded-lg shadow p-6">
                    <div className="flex items-center mb-6">
                      <div className="p-3 rounded-lg bg-red-100">
                        <Shield className="w-6 h-6 text-red-600" />
                      </div>
                      <div className="ml-4">
                        <h3 className="text-lg font-semibold text-gray-900">Ограничение скорости</h3>
                        <p className="text-sm text-gray-500">Защита от DDoS и лимиты</p>
                      </div>
                    </div>
                    
                    <div className="space-y-4">
                      <div className="flex justify-between items-center">
                        <span className="text-gray-600">Активные блокировки</span>
                        <span className="font-semibold">{metrics?.rate_limiting.active_bans || 0}</span>
                      </div>
                      <div className="flex justify-between items-center">
                        <span className="text-gray-600">Всего заблокировано</span>
                        <span className="font-semibold">{metrics?.rate_limiting.total_banned || 0}</span>
                      </div>
                      <div className="flex justify-between items-center">
                        <span className="text-gray-600">Запросов в минуту</span>
                        <span className="font-semibold">{metrics?.rate_limiting.requests_per_minute?.toFixed(1) || '0.0'}</span>
                      </div>
                    </div>
                  </div>

                  {/* Memory Usage */}
                  <div className="bg-white rounded-lg shadow p-6">
                    <div className="flex items-center mb-6">
                      <div className="p-3 rounded-lg bg-indigo-100">
                        <Database className="w-6 h-6 text-indigo-600" />
                      </div>
                      <div className="ml-4">
                        <h3 className="text-lg font-semibold text-gray-900">Использование памяти</h3>
                        <p className="text-sm text-gray-500">Потребление памяти сервером</p>
                      </div>
                    </div>
                    
                    <div className="space-y-4">
                      <div className="flex justify-between items-center">
                        <span className="text-gray-600">Используется памяти</span>
                        <span className="font-semibold">
                          {metrics?.system_info.memory_usage ? formatBytes(metrics.system_info.memory_usage) : '0 Б'}
                        </span>
                      </div>
                      <div className="w-full bg-gray-200 rounded-full h-2.5">
                        <div 
                          className="bg-indigo-600 h-2.5 rounded-full" 
                          style={{ 
                            width: `${Math.min(100, (metrics?.system_info.memory_usage || 0) / (100 * 1024 * 1024) * 100)}%` 
                          }}
                        />
                      </div>
                      <p className="text-sm text-gray-500">
                        Примерно {Math.round((metrics?.system_info.memory_usage || 0) / (1024 * 1024))} МБ из ~100 МБ
                      </p>
                    </div>
                  </div>
                </div>

                {/* Request Methods Breakdown */}
                <div className="bg-white rounded-lg shadow p-6 mb-8">
                  <div className="flex items-center mb-6">
                    <div className="p-3 rounded-lg bg-teal-100">
                      <Network className="w-6 h-6 text-teal-600" />
                    </div>
                    <div className="ml-4">
                      <h3 className="text-lg font-semibold text-gray-900">Распределение по методам</h3>
                      <p className="text-sm text-gray-500">Количество запросов по HTTP-методам</p>
                    </div>
                  </div>
                  
                  <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
                    {metrics?.request_counts && Object.entries(metrics.request_counts).map(([method, count]) => (
                      <div key={method} className="bg-gray-50 rounded-lg p-4">
                        <div className="text-sm font-medium text-gray-500">{method}</div>
                        <div className="text-2xl font-bold text-gray-900 mt-1">{count}</div>
                      </div>
                    ))}
                    {(!metrics?.request_counts || Object.keys(metrics.request_counts).length === 0) && (
                      <div className="col-span-4 text-center text-gray-500 py-8">
                        Нет данных о методах запросов
                      </div>
                    )}
                  </div>
                </div>
              </>
            )}
          </div>
        </main>
      </div>
    </div>
  );
};

// Refresh icon component
const Refresh: React.FC<{ className?: string }> = ({ className }) => (
  <svg 
    className={className} 
    fill="none" 
    stroke="currentColor" 
    viewBox="0 0 24 24" 
    xmlns="http://www.w3.org/2000/svg"
  >
    <path 
      strokeLinecap="round" 
      strokeLinejoin="round" 
      strokeWidth={2} 
      d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" 
    />
  </svg>
);