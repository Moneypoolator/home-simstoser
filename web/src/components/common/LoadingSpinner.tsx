import React from 'react';

interface LoadingSpinnerProps {
  size?: 'sm' | 'md' | 'lg' | 'xl';
  color?: 'primary' | 'gray' | 'white';
  text?: string;
  fullscreen?: boolean;
}

export const LoadingSpinner: React.FC<LoadingSpinnerProps> = ({
  size = 'md',
  color = 'primary',
  text,
  fullscreen = false,
}) => {
  const sizeClasses = {
    sm: 'h-4 w-4 border-2',
    md: 'h-8 w-8 border-2',
    lg: 'h-12 w-12 border-3',
    xl: 'h-16 w-16 border-4',
  };

  const colorClasses = {
    primary: 'border-primary-200 border-t-primary-600',
    gray: 'border-gray-200 border-t-gray-600',
    white: 'border-white border-opacity-30 border-t-white',
  };

  const wrapperClasses = fullscreen
    ? 'fixed inset-0 flex items-center justify-center bg-white bg-opacity-80 z-50'
    : 'flex items-center justify-center';

  return (
    <div className={wrapperClasses}>
      <div className="flex flex-col items-center">
        <div
          className={`animate-spin rounded-full ${sizeClasses[size]} ${colorClasses[color]}`}
        />
        {text && <p className="mt-2 text-sm text-gray-600">{text}</p>}
      </div>
    </div>
  );
};