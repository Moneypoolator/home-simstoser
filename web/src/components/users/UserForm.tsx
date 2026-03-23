import React, { useState } from 'react';
import { User, UserRole } from '../../types/api';
import { Button } from '../common/Button';

interface UserFormProps {
  user?: User;
  onSubmit: (username: string, email: string, role: UserRole) => void;
  onCancel: () => void;
}

const ROLES: { value: UserRole; label: string; description: string }[] = [
  { value: 'ADMIN', label: 'Администратор', description: 'Полный доступ ко всему' },
  { value: 'MANAGER', label: 'Менеджер', description: 'Управление файлами, но не настройками' },
  { value: 'CONTRIBUTOR', label: 'Автор', description: 'Загрузка и чтение своих файлов' },
  { value: 'VIEWER', label: 'Читатель', description: 'Только чтение' },
  { value: 'GUEST', label: 'Гость', description: 'Ограниченный доступ' },
];

export const UserForm: React.FC<UserFormProps> = ({ user, onSubmit, onCancel }) => {
  const [username, setUsername] = useState(user?.username || '');
  const [email, setEmail] = useState(user?.email || '');
  const [role, setRole] = useState<UserRole>(user?.role || 'VIEWER');
  const [errors, setErrors] = useState<{ username?: string; email?: string }>({});

  const validate = (): boolean => {
    const newErrors: { username?: string; email?: string } = {};

    if (!username.trim()) {
      newErrors.username = 'Имя пользователя обязательно';
    } else if (username.length < 3) {
      newErrors.username = 'Имя пользователя должно быть не менее 3 символов';
    }

    if (!email.trim()) {
      newErrors.email = 'Email обязателен';
    } else if (!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)) {
      newErrors.email = 'Неверный формат email';
    }

    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    if (validate()) {
      onSubmit(username.trim(), email.trim(), role);
    }
  };

  return (
    <form onSubmit={handleSubmit} className="space-y-4">
      <div>
        <label htmlFor="username" className="block text-sm font-medium text-gray-700">
          Имя пользователя
        </label>
        <input
          type="text"
          id="username"
          value={username}
          onChange={(e) => setUsername(e.target.value)}
          className={`mt-1 block w-full rounded-md border ${
            errors.username ? 'border-red-500' : 'border-gray-300'
          } shadow-sm focus:border-primary-500 focus:ring-primary-500`}
          disabled={!!user}
        />
        {errors.username && (
          <p className="mt-1 text-sm text-red-600">{errors.username}</p>
        )}
      </div>

      <div>
        <label htmlFor="email" className="block text-sm font-medium text-gray-700">
          Email
        </label>
        <input
          type="email"
          id="email"
          value={email}
          onChange={(e) => setEmail(e.target.value)}
          className={`mt-1 block w-full rounded-md border ${
            errors.email ? 'border-red-500' : 'border-gray-300'
          } shadow-sm focus:border-primary-500 focus:ring-primary-500`}
        />
        {errors.email && (
          <p className="mt-1 text-sm text-red-600">{errors.email}</p>
        )}
      </div>

      <div>
        <label htmlFor="role" className="block text-sm font-medium text-gray-700">
          Роль
        </label>
        <select
          id="role"
          value={role}
          onChange={(e) => setRole(e.target.value as UserRole)}
          className="mt-1 block w-full rounded-md border border-gray-300 bg-white shadow-sm focus:border-primary-500 focus:ring-primary-500"
        >
          {ROLES.map((r) => (
            <option key={r.value} value={r.value}>
              {r.label}
            </option>
          ))}
        </select>
        <p className="mt-1 text-sm text-gray-500">
          {ROLES.find((r) => r.value === role)?.description}
        </p>
      </div>

      <div className="flex justify-end space-x-3 pt-4">
        <Button variant="secondary" onClick={onCancel} type="button">
          Отмена
        </Button>
        <Button type="submit">
          {user ? 'Сохранить изменения' : 'Создать пользователя'}
        </Button>
      </div>
    </form>
  );
};