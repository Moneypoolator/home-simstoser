import React from 'react';
import { NavLink } from 'react-router-dom';
import { Home, Folder, Users, Key, Shield, Settings, Activity } from 'lucide-react';

const navigation = [
  { name: 'Панель управления', href: '/', icon: Home },
  { name: 'Файлы', href: '/files', icon: Folder },
  { name: 'Пользователи', href: '/users', icon: Users },
  { name: 'Ключи доступа', href: '/keys', icon: Key },
  { name: 'Политики', href: '/policies', icon: Shield },
  { name: 'Мониторинг', href: '/monitoring', icon: Activity },
  { name: 'Настройки', href: '/settings', icon: Settings },
];

export const Sidebar: React.FC = () => {
  return (
    <div className="w-64 bg-gray-800 text-white min-h-screen">
      <div className="p-4">
        <nav className="mt-4">
          {navigation.map((item) => {
            const Icon = item.icon;
            return (
              <NavLink
                key={item.href}
                to={item.href}
                className={({ isActive }) =>
                  `flex items-center px-4 py-3 text-sm font-medium rounded-lg transition-colors ${
                    isActive
                      ? 'bg-primary-600 text-white'
                      : 'text-gray-300 hover:bg-gray-700'
                  }`
                }
              >
                <Icon className="w-5 h-5 mr-3" />
                {item.name}
              </NavLink>
            );
          })}
        </nav>
      </div>
    </div>
  );
};