import api from './api';
import type { LoginResponse } from '../types/api';

export const authService = {
  async login(username: string, password: string): Promise<LoginResponse> {
    // В реальной реализации здесь будет запрос к серверу
    // для получения ключей доступа
    const response = await api.post('/auth/login', {
      username,
      password,
    });
    return response.data;
  },

  logout() {
    localStorage.removeItem('accessKeyId');
    localStorage.removeItem('secretAccessKey');
    localStorage.removeItem('userId');
    localStorage.removeItem('username');
  },

  isAuthenticated(): boolean {
    return !!localStorage.getItem('accessKeyId');
  },

  getCurrentUser() {
    return {
      userId: localStorage.getItem('userId'),
      username: localStorage.getItem('username'),
    };
  },
};