import api from './api';
import type { AccessKey } from '../types/api';

export const keysService = {
  async listKeys(): Promise<AccessKey[]> {
    const response = await api.get('/admin/keys');
    return response.data.keys;
  },

  async createKey(username: string): Promise<AccessKey> {
    const response = await api.post('/admin/keys', {
      username,
    });
    return response.data;
  },

  async deleteKey(accessKeyId: string): Promise<void> {
    await api.delete(`/admin/keys/${accessKeyId}`);
  },

  async activateKey(accessKeyId: string): Promise<AccessKey> {
    const response = await api.post(`/admin/keys/${accessKeyId}/activate`);
    return response.data;
  },

  async deactivateKey(accessKeyId: string): Promise<AccessKey> {
    const response = await api.post(`/admin/keys/${accessKeyId}/deactivate`);
    return response.data;
  },
};