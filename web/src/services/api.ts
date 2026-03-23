import axios from 'axios';

const API_BASE_URL = (import.meta as any).env?.VITE_API_URL || '/api';

const api = axios.create({
  baseURL: API_BASE_URL,
  headers: {
    'Content-Type': 'application/json',
  },
});

// Интерцептор для добавления заголовков аутентификации
api.interceptors.request.use((config) => {
  const accessKeyId = localStorage.getItem('accessKeyId');
  const secretAccessKey = localStorage.getItem('secretAccessKey');
  
  if (accessKeyId && secretAccessKey) {
    // Здесь будет реализация AWS Signature v4
    // Пока просто добавляем заголовки для тестирования
    config.headers['X-Access-Key'] = accessKeyId;
  }
  
  return config;
});

// Интерцептор для обработки ошибок
api.interceptors.response.use(
  (response) => response,
  (error) => {
    if (error.response?.status === 401) {
      // Токен недействителен, перенаправляем на логин
      localStorage.removeItem('accessKeyId');
      localStorage.removeItem('secretAccessKey');
      window.location.href = '/login';
    }
    return Promise.reject(error);
  }
);

export default api;