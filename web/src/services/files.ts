import api from './api';
import type { FileInfo, FileMetadata } from '../types/api';

export const filesService = {
  async listFiles(): Promise<FileInfo> {
    const response = await api.get('/list');
    return response.data;
  },

  async uploadFile(filename: string, file: File): Promise<any> {
    const arrayBuffer = await file.arrayBuffer();
    const data = new Uint8Array(arrayBuffer);
    
    const response = await api.put(`/${filename}`, data, {
      headers: {
        'Content-Type': file.type || 'application/octet-stream',
      },
    });
    return response.data;
  },

  async downloadFile(filename: string): Promise<Blob> {
    const response = await api.get(`/${filename}`, {
      responseType: 'blob',
    });
    return response.data;
  },

  async deleteFile(filename: string): Promise<any> {
    const response = await api.delete(`/${filename}`);
    return response.data;
  },

  async getFileMetadata(filename: string): Promise<FileMetadata> {
    const response = await api.head(`/${filename}`);
    return response.data;
  },
};