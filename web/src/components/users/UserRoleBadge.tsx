import React from 'react';
import { UserRole } from '../../types/api';
import { Crown, Briefcase, Eye, User } from 'lucide-react';

interface UserRoleBadgeProps {
  role: UserRole;
  size?: 'sm' | 'md' | 'lg';
}

const ROLE_CONFIG = {
  ADMIN: {
    label: 'Администратор',
    color: 'bg-red-100 text-red-800 border-red-200',
    icon: Crown,
  },
  MANAGER: {
    label: 'Менеджер',
    color: 'bg-orange-100 text-orange-800 border-orange-200',
    icon: Briefcase,
  },
  CONTRIBUTOR: {
    label: 'Автор',
    color: 'bg-blue-100 text-blue-800 border-blue-200',
    icon: User,
  },
  VIEWER: {
    label: 'Читатель',
    color: 'bg-green-100 text-green-800 border-green-200',
    icon: Eye,
  },
  GUEST: {
    label: 'Гость',
    color: 'bg-gray-100 text-gray-800 border-gray-200',
    icon: User,
  },
};

export const UserRoleBadge: React.FC<UserRoleBadgeProps> = ({ role, size = 'md' }) => {
  const config = ROLE_CONFIG[role];
  const Icon = config.icon;

  const sizeClasses = {
    sm: 'px-2 py-0.5 text-xs',
    md: 'px-2.5 py-0.5 text-sm',
    lg: 'px-3 py-1 text-base',
  };

  return (
    <span className={`inline-flex items-center rounded-full border ${config.color} ${sizeClasses[size]}`}>
      <Icon className="w-3 h-3 mr-1" />
      {config.label}
    </span>
  );
};