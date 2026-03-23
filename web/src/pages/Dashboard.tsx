import React, { useEffect, useState } from 'react';
import { Header } from '../components/layout/Header';
import { Sidebar } from '../components/layout/Sidebar';
import { LoadingSpinner } from '../components/common/LoadingSpinner';
import { FileList } from '../components/files/FileList';
import { FileUpload } from '../components/files/FileUpload';
import { FileMetadata } from '../types/api';
import { filesService } from '../services/files';
import { formatBytes } from '../utils/format';
import { Folder, FileText, Activity, TrendingUp, Users, KeyRound } from 'lucide-react';

export const Dashboard: React.FC = () => {
  const [files, setFiles] = useState<FileMetadata[]>([]);
  const [loading, setLoading] = useState(true);
  const [selectedFile, setSelectedFile] = useState<FileMetadata | null>(null);
  const [stats, setStats] = useState({
    totalFiles: 0,
    totalSize: 0,
    recentFiles: 0,
  });

  useEffect(() => {
    loadDashboardData();
  }, []);

  const loadDashboardData = async () => {
    try {
      setLoading(true);
      const data = await filesService.listFiles();
      setFiles(data.files || []);
      
      const now = new Date();
      const recentFiles = data.files?.filter(f => {
        const fileDate = new Date(f.last_modified);
        const diffTime = Math.abs(now.getTime() - fileDate.getTime());
        const diffDays = Math.ceil(diffTime / (1000 * 60 * 60 * 24));
        return diffDays <= 7;
      }) || [];

      setStats({
        totalFiles: data.files?.length || 0,
        totalSize: data.files?.reduce((sum, f) => sum + f.size, 0) || 0,
        recentFiles: recentFiles.length,
      });
    } catch (error) {
      console.error('Error loading dashboard data:', error);
    } finally {
      setLoading(false);
    }
  };

  const handleFileSelect = (file: FileMetadata) => {
    setSelectedFile(file);
  };

  const handleClosePreview = () => {
    setSelectedFile(null);
  };

  return (
    <div className="flex h-screen overflow-hidden bg-gray-50">
      <Sidebar />
      <div className="flex-1 flex flex-col overflow-hidden">
        <Header />
        
        <main className="flex-1 overflow-y-auto p-6">
          <div className="max-w-7xl mx-auto">
            {/* Header */}
            <div className="mb-8">
              <h1 className="text-3xl font-bold text-gray-900">Панель управления</h1>
              <p className="text-gray-500 mt-2">Обзор состояния вашего хранилища</p>
            </div>

            {/* Stats Grid */}
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6 mb-8">
              <div className="bg-white rounded-lg shadow p-6">
                <div className="flex items-center">
                  <div className="p-3 rounded-lg bg-primary-100">
                    <Folder className="w-6 h-6 text-primary-600" />
                  </div>
                  <div className="ml-4">
                    <p className="text-sm font-medium text-gray-600">Всего файлов</p>
                    <p className="text-2xl font-bold text-gray-900">{stats.totalFiles}</p>
                  </div>
                </div>
              </div>

              <div className="bg-white rounded-lg shadow p-6">
                <div className="flex items-center">
                  <div className="p-3 rounded-lg bg-green-100">
                    <FileText className="w-6 h-6 text-green-600" />
                  </div>
                  <div className="ml-4">
                    <p className="text-sm font-medium text-gray-600">Общий размер</p>
                    <p className="text-2xl font-bold text-gray-900">{formatBytes(stats.totalSize)}</p>
                  </div>
                </div>
              </div>

              <div className="bg-white rounded-lg shadow p-6">
                <div className="flex items-center">
                  <div className="p-3 rounded-lg bg-blue-100">
                    <Activity className="w-6 h-6 text-blue-600" />
                  </div>
                  <div className="ml-4">
                    <p className="text-sm font-medium text-gray-600">Новые (7 дней)</p>
                    <p className="text-2xl font-bold text-gray-900">{stats.recentFiles}</p>
                  </div>
                </div>
              </div>

              <div className="bg-white rounded-lg shadow p-6">
                <div className="flex items-center">
                  <div className="p-3 rounded-lg bg-purple-100">
                    <TrendingUp className="w-6 h-6 text-purple-600" />
                  </div>
                  <div className="ml-4">
                    <p className="text-sm font-medium text-gray-600">Использование</p>
                    <p className="text-2xl font-bold text-gray-900">~</p>
                  </div>
                </div>
              </div>
            </div>

            {/* Quick Actions */}
            <div className="bg-white rounded-lg shadow p-6 mb-8">
              <h2 className="text-lg font-semibold text-gray-900 mb-4">Быстрые действия</h2>
              <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
                <div className="p-4 bg-blue-50 rounded-lg cursor-pointer hover:bg-blue-100 transition-colors">
                  <div className="flex items-center">
                    <Folder className="w-5 h-5 text-blue-600 mr-2" />
                    <span className="text-blue-800 font-medium">Просмотр файлов</span>
                  </div>
                </div>
                <div className="p-4 bg-green-50 rounded-lg cursor-pointer hover:bg-green-100 transition-colors">
                  <div className="flex items-center">
                    <Users className="w-5 h-5 text-green-600 mr-2" />
                    <span className="text-green-800 font-medium">Управление пользователями</span>
                  </div>
                </div>
                <div className="p-4 bg-purple-50 rounded-lg cursor-pointer hover:bg-purple-100 transition-colors">
                  <div className="flex items-center">
                    <KeyRound className="w-5 h-5 text-purple-600 mr-2" />
                    <span className="text-purple-800 font-medium">Ключи доступа</span>
                  </div>
                </div>
              </div>
            </div>

            {/* File Upload */}
            <div className="mb-8">
              <FileUpload />
            </div>

            {/* Recent Files */}
            <div className="bg-white rounded-lg shadow overflow-hidden">
              <div className="px-6 py-4 border-b">
                <h2 className="text-lg font-semibold text-gray-900">Последние файлы</h2>
              </div>
              <div className="p-6">
                {loading ? (
                  <LoadingSpinner text="Загрузка файлов..." />
                ) : files.length === 0 ? (
                  <div className="text-center py-12">
                    <FileText className="w-16 h-16 mx-auto text-gray-400" />
                    <p className="mt-2 text-gray-600">Нет файлов в хранилище</p>
                  </div>
                ) : (
                  <FileList onFileSelect={handleFileSelect} />
                )}
              </div>
            </div>
          </div>
        </main>
      </div>

      {/* File Preview Modal */}
      {selectedFile && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50 p-4">
          <div className="bg-white rounded-lg shadow-2xl max-w-4xl w-full max-h-[90vh] flex flex-col">
            <div className="flex items-center justify-between p-4 border-b">
              <div>
                <h3 className="text-lg font-semibold text-gray-900">{selectedFile.name}</h3>
                <p className="text-sm text-gray-500 mt-1">{formatBytes(selectedFile.size)}</p>
              </div>
              <button
                onClick={handleClosePreview}
                className="text-gray-400 hover:text-gray-600"
              >
                <svg className="w-6 h-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" />
                </svg>
              </button>
            </div>
            <div className="flex-1 overflow-auto p-4 text-center">
              <FileText className="w-24 h-24 mx-auto text-gray-400" />
              <p className="text-gray-600 mt-4">Предпросмотр недоступен</p>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};