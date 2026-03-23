import React, { useState } from 'react';
import { Button } from '../components/common/Button';
import { useNavigate, useLocation } from 'react-router-dom';
import { Lock, User, LogIn, AlertCircle } from 'lucide-react';
import { storageUtils } from '../utils/storage';

export const LoginPage: React.FC = () => {
  const navigate = useNavigate();
  const location = useLocation();
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');

  const from = (location.state as any)?.from?.pathname || '/';

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError('');

    if (!username || !password) {
      setError('Пожалуйста, заполните все поля');
      return;
    }

    setLoading(true);

    try {
      // В реальной реализации здесь будет вызов сервера
      // для получения ключей доступа
      // Пока просто используем тестовые данные
      
      // Это нужно заменить на реальный вызов:
      // const response = await authService.login(username, password);
      
      // Для тестирования:
      const testAccessKeyId = 'AKIAIOSFODNN7EXAMPLE';
      const testSecretKey = 'wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY';
      
      storageUtils.saveAuthToken(testAccessKeyId, testSecretKey);
      storageUtils.saveUserInfo('test-user-id', username);
      
      navigate(from, { replace: true });
    } catch (err: any) {
      setError(err.response?.data?.message || 'Ошибка аутентификации. Проверьте логин и пароль.');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="min-h-screen bg-gradient-to-br from-primary-50 to-primary-100 flex flex-col justify-center py-12 sm:px-6 lg:px-8">
      <div className="sm:mx-auto sm:w-full sm:max-w-md">
        <div className="text-center">
          <div className="inline-flex items-center justify-center w-16 h-16 rounded-full bg-primary-100">
            <LogIn className="w-8 h-8 text-primary-600" />
          </div>
          <h2 className="mt-6 text-3xl font-bold text-gray-900">S3 Storage</h2>
          <p className="mt-2 text-sm text-gray-600">
            Войдите для доступа к хранилищу
          </p>
        </div>
      </div>

      <div className="mt-8 sm:mx-auto sm:w-full sm:max-w-md">
        <div className="bg-white py-8 px-4 shadow-xl sm:rounded-lg sm:px-10">
          <form onSubmit={handleSubmit} className="space-y-6">
            <div>
              <label htmlFor="username" className="block text-sm font-medium text-gray-700">
                Имя пользователя
              </label>
              <div className="mt-1 relative">
                <div className="absolute inset-y-0 left-0 pl-3 flex items-center pointer-events-none">
                  <User className="h-5 w-5 text-gray-400" />
                </div>
                <input
                  id="username"
                  name="username"
                  type="text"
                  autoComplete="username"
                  value={username}
                  onChange={(e) => setUsername(e.target.value)}
                  className="pl-10 appearance-none block w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm placeholder-gray-400 focus:outline-none focus:ring-primary-500 focus:border-primary-500"
                  autoFocus
                />
              </div>
            </div>

            <div>
              <label htmlFor="password" className="block text-sm font-medium text-gray-700">
                Пароль
              </label>
              <div className="mt-1 relative">
                <div className="absolute inset-y-0 left-0 pl-3 flex items-center pointer-events-none">
                  <Lock className="h-5 w-5 text-gray-400" />
                </div>
                <input
                  id="password"
                  name="password"
                  type="password"
                  autoComplete="current-password"
                  value={password}
                  onChange={(e) => setPassword(e.target.value)}
                  className="pl-10 appearance-none block w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm placeholder-gray-400 focus:outline-none focus:ring-primary-500 focus:border-primary-500"
                />
              </div>
            </div>

            {error && (
              <div className="rounded-md bg-red-50 p-4">
                <div className="flex">
                  <div className="flex-shrink-0">
                    <AlertCircle className="h-5 w-5 text-red-400" />
                  </div>
                  <div className="ml-3">
                    <p className="text-sm text-red-700">{error}</p>
                  </div>
                </div>
              </div>
            )}

            <div>
              <Button type="submit" className="w-full" isLoading={loading}>
                <LogIn className="w-4 h-4 mr-2" />
                Войти
              </Button>
            </div>
          </form>

          <div className="mt-6">
            <div className="relative">
              <div className="absolute inset-0 flex items-center">
                <div className="w-full border-t border-gray-300"></div>
              </div>
              <div className="relative flex justify-center text-sm">
                <span className="px-2 bg-white text-gray-500">
                  или
                </span>
              </div>
            </div>

            <div className="mt-6">
              <p className="text-center text-sm text-gray-600">
                Тестовые учетные данные: <br />
                <span className="font-mono">username: admin</span> <br />
                <span className="font-mono">password: admin123</span>
              </p>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};