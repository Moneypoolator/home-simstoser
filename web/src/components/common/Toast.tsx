import React, { createContext, useContext, useState, useEffect } from 'react';
import { CheckCircle, XCircle, AlertTriangle, Info } from 'lucide-react';
import { Button } from './Button';

type ToastType = 'success' | 'error' | 'warning' | 'info';

interface Toast {
  id: number;
  type: ToastType;
  title: string;
  message: string;
  duration?: number;
}

interface ToastContextType {
  showToast: (toast: Omit<Toast, 'id'>) => void;
  hideToast: (id: number) => void;
}

const ToastContext = createContext<ToastContextType | undefined>(undefined);

export const ToastProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const [toasts, setToasts] = useState<Toast[]>([]);

  const showToast = (toast: Omit<Toast, 'id'>) => {
    const id = Date.now();
    setToasts((prev) => [...prev, { ...toast, id }]);
    
    // Auto-hide after duration
    if (toast.duration !== undefined) {
      setTimeout(() => hideToast(id), toast.duration);
    }
  };

  const hideToast = (id: number) => {
    setToasts((prev) => prev.filter((t) => t.id !== id));
  };

  return (
    <ToastContext.Provider value={{ showToast, hideToast }}>
      {children}
      <ToastContainer toasts={toasts} onHide={hideToast} />
    </ToastContext.Provider>
  );
};

interface ToastContainerProps {
  toasts: Toast[];
  onHide: (id: number) => void;
}

const ToastContainer: React.FC<ToastContainerProps> = ({ toasts, onHide }) => {
  return (
    <div className="fixed top-4 right-4 z-50 space-y-2 max-w-md">
      {toasts.map((toast) => (
        <ToastItem key={toast.id} toast={toast} onHide={onHide} />
      ))}
    </div>
  );
};

interface ToastItemProps {
  toast: Toast;
  onHide: (id: number) => void;
}

const ToastItem: React.FC<ToastItemProps> = ({ toast, onHide }) => {
  const [isVisible, setIsVisible] = useState(true);

  useEffect(() => {
    if (toast.duration) {
      const timer = setTimeout(() => {
        setIsVisible(false);
      }, toast.duration - 500); // Start fade out 500ms before removal
      
      return () => clearTimeout(timer);
    }
  }, [toast.duration]);

  const typeConfig = {
    success: {
      icon: CheckCircle,
      color: 'bg-green-50 border-green-500 text-green-800',
      iconColor: 'text-green-400',
    },
    error: {
      icon: XCircle,
      color: 'bg-red-50 border-red-500 text-red-800',
      iconColor: 'text-red-400',
    },
    warning: {
      icon: AlertTriangle,
      color: 'bg-yellow-50 border-yellow-500 text-yellow-800',
      iconColor: 'text-yellow-400',
    },
    info: {
      icon: Info,
      color: 'bg-blue-50 border-blue-500 text-blue-800',
      iconColor: 'text-blue-400',
    },
  };

  const config = typeConfig[toast.type];
  const Icon = config.icon;

  return (
    <div
      className={`flex items-start p-4 rounded-lg border-l-4 shadow-lg transform transition-all ${
        config.color
      } ${isVisible ? 'opacity-100 translate-x-0' : 'opacity-0 translate-x-4'}`}
    >
      <div className={`flex-shrink-0 ${config.iconColor}`}>
        <Icon className="h-5 w-5" />
      </div>
      <div className="ml-3 flex-1">
        <p className="text-sm font-medium">{toast.title}</p>
        {toast.message && (
          <p className="text-sm mt-1">{toast.message}</p>
        )}
      </div>
      <div className="ml-4 flex-shrink-0">
        <Button
          variant="ghost"
          size="sm"
          onClick={() => onHide(toast.id)}
          className="text-current hover:bg-current/10"
        >
          ✕
        </Button>
      </div>
    </div>
  );
};

export const useToast = () => {
  const context = useContext(ToastContext);
  if (context === undefined) {
    throw new Error('useToast must be used within a ToastProvider');
  }
  return context;
};