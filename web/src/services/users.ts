import api from './api';
import type { User, UserRole } from '../types/api';

export const usersService = {
  async listUsers(): Promise<User[]> {
    const response = await api.get('/admin/users');
    return response.data.users;
  },

  async createUser(
    username: string,
    email: string,
    role: UserRole = 'VIEWER'
  ): Promise<User> {
    const response = await api.post('/admin/users', {
      username,
      email,
      role,
    });
    return response.data;
  },

  async updateUserRole(userId: string, role: UserRole): Promise<User> {
    const response = await api.put(`/admin/users/${userId}/role`, {
      role,
    });
    return response.data;
  },

  async activateUser(userId: string): Promise<User> {
    const response = await api.post(`/admin/users/${userId}/activate`);
    return response.data;
  },

  async deactivateUser(userId: string): Promise<User> {
    const response = await api.post(`/admin/users/${userId}/deactivate`);
    return response.data;
  },

  async deleteUser(userId: string): Promise<void> {
    await api.delete(`/admin/users/${userId}`);
  },
};