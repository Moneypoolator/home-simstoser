import React, { useState } from 'react';
import { Header } from '../components/layout/Header';
import { Sidebar } from '../components/layout/Sidebar';
import { Button } from '../components/common/Button';
import { authService } from '../services/auth';
import { useNavigate } from 'react-router-dom';
import { 
  Settings, 
  Shield,    
  Bell, 
  Terminal, 
  Save, 
  RotateCcw,
  Moon,
  Sun
} from 'lucide-react';

export const SettingsPage: React.FC = () => {
  const navigate = useNavigate();
  const currentUser = authService.getCurrentUser();
  
  const [storagePath, setStoragePath] = useState(localStorage.getItem('storagePath') || './storage');
  const [maxFileSize, setMaxFileSize] = useState(localStorage.getItem('maxFileSize') || '100');
  const [theme, setTheme] = useState(localStorage.getItem('theme') || 'light');
  const [notifications, setNotifications] = useState(localStorage.getItem('notifications') === 'true');
  const [autoRefresh, setAutoRefresh] = useState(localStorage.getItem('autoRefresh') === 'true');
  const [saving, setSaving] = useState(false);

  const handleSaveSettings = () => {
    setSaving(true);
    try {
      localStorage.setItem('storagePath', storagePath);
      localStorage.setItem('maxFileSize', maxFileSize);
      localStorage.setItem('theme', theme);
      localStorage.setItem('notifications', String(notifications));
      localStorage.setItem('autoRefresh', String(autoRefresh));
      
      // Применение темы
      document.documentElement.classList.remove('dark', 'light');
      document.documentElement.classList.add(theme);
      
      alert('Настройки успешно сохранены');
    } catch (error) {
      console.error('Error saving settings:', error);
      alert('Ошибка при сохранении настроек');
    } finally {
      setSaving(false);
    }
  };

  const handleResetSettings = () => {
    if (confirm('Сбросить все настройки до значений по умолчанию?')) {
      localStorage.removeItem('storagePath');
      localStorage.removeItem('maxFileSize');
      localStorage.removeItem('theme');
      localStorage.removeItem('notifications');
      localStorage.removeItem('autoRefresh');
      
      setStoragePath('./storage');
      setMaxFileSize('100');
      setTheme('light');
      setNotifications(true);
      setAutoRefresh(true);
      
      document.documentElement.classList.remove('dark', 'light');
      document.documentElement.classList.add('light');
      
      alert('Настройки сброшены');
    }
  };

  const handleLogout = () => {
    authService.logout();
    navigate('/login');
  };

  return (
    <div className="flex h-screen overflow-hidden bg-gray-50">
      <Sidebar />
      <div className="flex-1 flex flex-col overflow-hidden">
        <Header />
        <main className="flex-1 overflow-y-auto p-6">
          <div className="max-w-4xl mx-auto">
            <div className="mb-8">
              <h1 className="text-3xl font-bold text-gray-900">Настройки</h1>
              <p className="text-gray-500 mt-2">Настройте ваше хранилище</p>
            </div>

            {/* General Settings */}
            <div className="bg-white rounded-lg shadow mb-6">
              <div className="px-6 py-4 border-b flex items-center">
                <Settings className="w-5 h-5 text-primary-600 mr-2" />
                <h2 className="text-lg font-semibold text-gray-900">Общие настройки</h2>
              </div>
              <div className="p-6 space-y-6">
                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-2">
                    Путь к хранилищу
                  </label>
                  <input
                    type="text"
                    value={storagePath}
                    onChange={(e) => setStoragePath(e.target.value)}
                    className="w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-primary-500 focus:border-primary-500"
                  />
                  <p className="mt-1 text-sm text-gray-500">
                    Директория для хранения файлов
                  </p>
                </div>

                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-2">
                    Максимальный размер файла (МБ)
                  </label>
                  <input
                    type="number"
                    value={maxFileSize}
                    onChange={(e) => setMaxFileSize(e.target.value)}
                    className="w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-primary-500 focus:border-primary-500"
                    min="1"
                    max="10000"
                  />
                  <p className="mt-1 text-sm text-gray-500">
                    Максимальный размер загружаемого файла
                  </p>
                </div>
              </div>
            </div>

            {/* Appearance Settings */}
            <div className="bg-white rounded-lg shadow mb-6">
              <div className="px-6 py-4 border-b flex items-center">
                <Moon className="w-5 h-5 text-primary-600 mr-2" />
                <h2 className="text-lg font-semibold text-gray-900">Оформление</h2>
              </div>
              <div className="p-6 space-y-6">
                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-2">
                    Тема
                  </label>
                  <div className="flex space-x-4">
                    <label className="flex items-center cursor-pointer">
                      <input
                        type="radio"
                        name="theme"
                        value="light"
                        checked={theme === 'light'}
                        onChange={(e) => setTheme(e.target.value)}
                        className="form-radio text-primary-600"
                      />
                      <span className="ml-2 text-gray-700 flex items-center">
                        <Sun className="w-4 h-4 mr-1" />
                        Светлая
                      </span>
                    </label>
                    <label className="flex items-center cursor-pointer">
                      <input
                        type="radio"
                        name="theme"
                        value="dark"
                        checked={theme === 'dark'}
                        onChange={(e) => setTheme(e.target.value)}
                        className="form-radio text-primary-600"
                      />
                      <span className="ml-2 text-gray-700 flex items-center">
                        <Moon className="w-4 h-4 mr-1" />
                        Темная
                      </span>
                    </label>
                  </div>
                </div>
              </div>
            </div>

            {/* Notifications Settings */}
            <div className="bg-white rounded-lg shadow mb-6">
              <div className="px-6 py-4 border-b flex items-center">
                <Bell className="w-5 h-5 text-primary-600 mr-2" />
                <h2 className="text-lg font-semibold text-gray-900">Уведомления</h2>
              </div>
              <div className="p-6 space-y-6">
                <div className="flex items-center justify-between">
                  <div>
                    <p className="text-sm font-medium text-gray-900">Показывать уведомления</p>
                    <p className="text-sm text-gray-500 mt-1">Всплывающие уведомления о действиях</p>
                  </div>
                  <label className="relative inline-flex items-center cursor-pointer">
                    <input
                      type="checkbox"
                      checked={notifications}
                      onChange={(e) => setNotifications(e.target.checked)}
                      className="sr-only peer"
                    />
                    <div className="w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-primary-300 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-primary-600"></div>
                  </label>
                </div>

                <div className="flex items-center justify-between">
                  <div>
                    <p className="text-sm font-medium text-gray-900">Автообновление</p>
                    <p className="text-sm text-gray-500 mt-1">Автоматически обновлять список файлов</p>
                  </div>
                  <label className="relative inline-flex items-center cursor-pointer">
                    <input
                      type="checkbox"
                      checked={autoRefresh}
                      onChange={(e) => setAutoRefresh(e.target.checked)}
                      className="sr-only peer"
                    />
                    <div className="w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-primary-300 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-primary-600"></div>
                  </label>
                </div>
              </div>
            </div>

            {/* Account Settings */}
            <div className="bg-white rounded-lg shadow mb-6">
              <div className="px-6 py-4 border-b flex items-center">
                <Shield className="w-5 h-5 text-primary-600 mr-2" />
                <h2 className="text-lg font-semibold text-gray-900">Аккаунт</h2>
              </div>
              <div className="p-6 space-y-6">
                <div>
                  <p className="text-sm font-medium text-gray-900">Имя пользователя</p>
                  <p className="text-sm text-gray-500 mt-1">{currentUser.username || 'Неизвестно'}</p>
                </div>
                <div>
                  <p className="text-sm font-medium text-gray-900">User ID</p>
                  <p className="text-sm text-gray-500 mt-1">{currentUser.userId || 'Неизвестно'}</p>
                </div>
                <div>
                  <Button variant="danger" onClick={handleLogout}>
                    Выйти из системы
                  </Button>
                </div>
              </div>
            </div>

            {/* Actions */}
            <div className="bg-white rounded-lg shadow mb-6">
              <div className="px-6 py-4 border-b">
                <h2 className="text-lg font-semibold text-gray-900">Действия</h2>
              </div>
              <div className="p-6 space-y-4">
                <Button onClick={handleSaveSettings} isLoading={saving}>
                  <Save className="w-4 h-4 mr-2" />
                  Сохранить настройки
                </Button>
                <Button variant="secondary" onClick={handleResetSettings}>
                  <RotateCcw className="w-4 h-4 mr-2" />
                  Сбросить настройки
                </Button>
              </div>
            </div>

            {/* System Info */}
            <div className="bg-white rounded-lg shadow">
              <div className="px-6 py-4 border-b flex items-center">
                <Terminal className="w-5 h-5 text-primary-600 mr-2" />
                <h2 className="text-lg font-semibold text-gray-900">Системная информация</h2>
              </div>
              <div className="p-6">
                <div className="grid grid-cols-2 gap-4">
                  <div>
                    <p className="text-sm text-gray-500">Версия</p>
                    <p className="text-sm font-medium text-gray-900">1.0.0</p>
                  </div>
                  <div>
                    <p className="text-sm text-gray-500">Сборка</p>
                    <p className="text-sm font-medium text-gray-900">2026.03.20</p>
                  </div>
                  <div>
                    <p className="text-sm text-gray-500">Node.js</p>
                    <p className="text-sm font-medium text-gray-900">{typeof process !== 'undefined' ? process.version : 'Unknown'}</p>
                  </div>
                  <div>
                    <p className="text-sm text-gray-500">Браузер</p>
                    <p className="text-sm font-medium text-gray-900">{navigator.userAgent}</p>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </main>
      </div>
    </div>
  );
};