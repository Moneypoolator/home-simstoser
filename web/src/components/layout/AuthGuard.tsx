import React from 'react';
import { Navigate, useLocation } from 'react-router-dom';
import { authService } from '../../services/auth';

interface AuthGuardProps {
  children: React.ReactNode;
  requireAuth?: boolean;
  allowedRoles?: string[];
}

export const AuthGuard: React.FC<AuthGuardProps> = ({
  children,
  requireAuth = true,
  allowedRoles,
}) => {
  const location = useLocation();
  const isAuthenticated = authService.isAuthenticated();
  const currentUser = authService.getCurrentUser();

  // Пока проверяем аутентификацию
  if (requireAuth && !isAuthenticated) {
    return <Navigate to="/login" state={{ from: location }} replace />;
  }

  // Проверка ролей (если указаны)
  if (allowedRoles && isAuthenticated && currentUser.userId) {
    // Здесь нужно будет добавить проверку роли пользователя
    // Пока просто пропускаем всех аутентифицированных
  }

  // Если не требуется аутентификация и пользователь не залогинен
  if (!requireAuth && !isAuthenticated) {
    return <>{children}</>;
  }

  return <>{children}</>;
};

// HOC для защиты компонентов
export const withAuthGuard = (
  Component: React.ComponentType<any>,
  options?: { requireAuth?: boolean; allowedRoles?: string[] }
) => {
  return (props: any) => (
    <AuthGuard {...options}>
      <Component {...props} />
    </AuthGuard>
  );
};