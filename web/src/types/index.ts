export type { 
  FileMetadata, 
  FileInfo, 
  AccessKey, 
  User, 
  UserRole, 
  Permission, 
  PermissionType, 
  AccessPolicy,
  LoginResponse
} from './api';

export interface PaginatedResponse<T> {
  items: T[];
  total: number;
  page: number;
  pageSize: number;
  totalPages: number;
}

export interface SortOptions {
  field: string;
  direction: 'asc' | 'desc';
}

export interface FilterOptions {
  [key: string]: string | number | boolean | string[] | undefined;
}

export interface ErrorResponse {
  error: string;
  message: string;
  code?: string;
  details?: any;
}

export interface SuccessResponse<T = any> {
  success: boolean;
  data?: T;
  message?: string;
}

export type ApiResponse<T = any> = SuccessResponse<T> | ErrorResponse;

// Файловые операции
export interface UploadProgress {
  loaded: number;
  total: number;
  percent: number;
}

export interface FileUploadOptions {
  onProgress?: (progress: UploadProgress) => void;
  abortSignal?: AbortSignal;
}

// Пагинация
export interface PaginationState {
  page: number;
  pageSize: number;
  total: number;
}

// Сортировка
export interface SortState {
  field: string;
  direction: 'asc' | 'desc';
}

// Фильтрация
export interface FilterState {
  searchTerm?: string;
  status?: string;
  dateRange?: [Date | null, Date | null];
  [key: string]: any;
}

// Настройки приложения
export interface AppSettings {
  theme: 'light' | 'dark';
  language: string;
  notifications: boolean;
  autoRefresh: boolean;
  storagePath: string;
  maxFileSize: number;
}

// Статистика
export interface StorageStats {
  totalFiles: number;
  totalSize: number;
  usedSpace: number;
  availableSpace: number;
  recentFiles: number;
}

// События
export interface SystemEvent {
  id: string;
  type: string;
  message: string;
  timestamp: string;
  severity: 'info' | 'warning' | 'error' | 'success';
  details?: any;
}

// Конфигурация
export interface AppConfig {
  apiUrl: string;
  wsUrl?: string;
  maxUploadSize: number;
  supportedFileTypes: string[];
  features: {
    authentication: boolean;
    authorization: boolean;
    encryption: boolean;
    compression: boolean;
  };
}