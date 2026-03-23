import api from './api';
import type { AccessPolicy, Permission, PermissionType } from '../types/api';

export const policiesService = {
  async listPolicies(): Promise<AccessPolicy[]> {
    const response = await api.get('/admin/policies');
    return response.data.policies || [];
  },

  async createPolicy(
    name: string,
    description: string,
    permissions: Permission[]
  ): Promise<AccessPolicy> {
    const response = await api.post('/admin/policies', {
      name,
      description,
      permissions,
    });
    return response.data;
  },

  async getPolicy(policyId: string): Promise<AccessPolicy> {
    const response = await api.get(`/admin/policies/${policyId}`);
    return response.data;
  },

  async updatePolicy(
    policyId: string,
    name: string,
    description: string,
    permissions: Permission[]
  ): Promise<AccessPolicy> {
    const response = await api.put(`/admin/policies/${policyId}`, {
      name,
      description,
      permissions,
    });
    return response.data;
  },

  async deletePolicy(policyId: string): Promise<void> {
    await api.delete(`/admin/policies/${policyId}`);
  },

  async attachPolicyToUser(userId: string, policyId: string): Promise<void> {
    await api.post(`/admin/users/${userId}/policies`, {
      policy_id: policyId,
    });
  },

  async detachPolicyFromUser(userId: string, policyId: string): Promise<void> {
    await api.delete(`/admin/users/${userId}/policies/${policyId}`);
  },

  async listUserPolicies(userId: string): Promise<AccessPolicy[]> {
    const response = await api.get(`/admin/users/${userId}/policies`);
    return response.data.policies || [];
  },

  // Валидация разрешений
  validatePermissionType(type: string): type is PermissionType {
    return ['READ', 'WRITE', 'DELETE', 'LIST', 'MANAGE_ACL'].includes(type);
  },

  // Преобразование строки в тип разрешения
  parsePermissionType(str: string): PermissionType {
    const validTypes: PermissionType[] = ['READ', 'WRITE', 'DELETE', 'LIST', 'MANAGE_ACL'];
    if (validTypes.includes(str as PermissionType)) {
      return str as PermissionType;
    }
    throw new Error(`Invalid permission type: ${str}`);
  },

  // Преобразование типа разрешения в строку
  permissionTypeToString(type: PermissionType): string {
    return type;
  },
};