import React from 'react';
import { useNavigate } from 'react-router-dom';
import { Button } from '../common/Button';
import { authService } from '../../services/auth';

export const Header: React.FC = () => {
  const navigate = useNavigate();
  const currentUser = authService.getCurrentUser();

  const handleLogout = () => {
    authService.logout();
    navigate('/login');
  };

  return (
    <header className="bg-white shadow-sm">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="flex justify-between items-center h-16">
          <div className="flex items-center">
            <h1 className="text-2xl font-bold text-primary-600 cursor-pointer" onClick={() => navigate('/')}>
              S3 Storage
            </h1>
          </div>
          
          <div className="flex items-center space-x-4">
            {currentUser.username && (
              <span className="text-gray-700">
                {currentUser.username}
              </span>
            )}
            <Button variant="secondary" onClick={handleLogout}>
              Выйти
            </Button>
          </div>
        </div>
      </div>
    </header>
  );
};