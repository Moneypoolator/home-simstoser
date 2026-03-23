import React, { useState } from 'react';
import { usersService } from '../../services/users';
import { Button } from '../common/Button';

interface AccessKeyFormProps {
  onSubmit: (username: string) => void;
  onCancel: () => void;
}

export const AccessKeyForm: React.FC<AccessKeyFormProps> = ({ onSubmit, onCancel }) => {
  const [username, setUsername] = useState('');
  const [availableUsers, setAvailableUsers] = useState<string[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');

  React.useEffect(() => {
    loadUsers();
  }, []);

  const loadUsers = async () => {
    try {
      setLoading(true);
      const users = await usersService.listUsers();
      setAvailableUsers(users.filter(u => u.is_active).map(u => u.username));
    } catch (error) {
      console.error('Error loading users:', error);
    } finally {
      setLoading(false);
    }
  };

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    if (!username.trim()) {
      setError('Пожалуйста, выберите пользователя');
      return;
    }
    setError('');
    onSubmit(username.trim());
  };

  return (
    <form onSubmit={handleSubmit} className="space-y-4">
      <div>
        <label htmlFor="username" className="block text-sm font-medium text-gray-700">
          Пользователь
        </label>
        <select
          id="username"
          value={username}
          onChange={(e) => setUsername(e.target.value)}
          className="mt-1 block w-full rounded-md border border-gray-300 bg-white shadow-sm focus:border-primary-500 focus:ring-primary-500"
          disabled={loading}
        >
          <option value="">Выберите пользователя...</option>
          {availableUsers.map((user) => (
            <option key={user} value={user}>
              {user}
            </option>
          ))}
        </select>
        {error && <p className="mt-1 text-sm text-red-600">{error}</p>}
        {loading && <p className="mt-1 text-sm text-gray-500">Загрузка пользователей...</p>}
      </div>

      <div className="bg-blue-50 p-3 rounded-md">
        <p className="text-sm text-blue-800">
          <strong>Важно:</strong> Секретный ключ будет показан только один раз после создания. 
          Обязательно скопируйте его и сохраните в безопасном месте!
        </p>
      </div>

      <div className="flex justify-end space-x-3 pt-4">
        <Button variant="secondary" onClick={onCancel} type="button">
          Отмена
        </Button>
        <Button type="submit" disabled={!username || loading}>
          Создать ключ
        </Button>
      </div>
    </form>
  );
};