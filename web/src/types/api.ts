export interface FileMetadata {
  name: string;
  size: number;
  last_modified: string;
  etag: string;
}

export interface FileInfo {
  count: number;
  files: FileMetadata[];
}

export interface AccessKey {
  access_key_id: string;
  secret_access_key?: string; // Только при создании
  user_name: string;
  is_active: boolean;
  created_at: string;
}

export interface User {
  user_id: string;
  username: string;
  email: string;
  role: UserRole;
  is_active: boolean;
  created_at: string;
  last_login?: string;
}

export type UserRole = 'ADMIN' | 'MANAGER' | 'CONTRIBUTOR' | 'VIEWER' | 'GUEST';

export interface Permission {
  type: PermissionType;
  resource_pattern: string;
  allow: boolean;
}

export type PermissionType = 'READ' | 'WRITE' | 'DELETE' | 'LIST' | 'MANAGE_ACL';

export interface AccessPolicy {
  policy_id: string;
  name: string;
  description: string;
  permissions: Permission[];
  created_at: string;
}

export interface LoginResponse {
  access_key_id: string;
  secret_access_key: string;
  user_id: string;
  username: string;
}

export interface ApiResponse<T = any> {
  success?: boolean;
  data?: T;
  error?: string;
  message?: string;
}