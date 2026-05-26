import api from './api';

export interface MultipartInitiateResponse {
  upload_id: string;
  filename: string;
  message: string;
}

export interface MultipartPartUploadResponse {
  success: boolean;
  upload_id: string;
  part_number: number;
  size: number;
}

export interface MultipartCompleteRequest {
  parts: number[];
}

export interface MultipartCompleteResponse {
  success: boolean;
  upload_id: string;
  message: string;
}

export interface MultipartProgressResponse {
  upload_id: string;
  parts: Record<string, number>;
}

export interface MultipartAbortResponse {
  success: boolean;
  upload_id: string;
  message: string;
}

export const multipartService = {
  async initiateUpload(filename: string): Promise<MultipartInitiateResponse> {
    const response = await api.post(`/upload/initiate?filename=${encodeURIComponent(filename)}`);
    return response.data;
  },

  async uploadPart(uploadId: string, partNumber: number, data: ArrayBuffer): Promise<MultipartPartUploadResponse> {
    const response = await api.put(`/upload/part?upload_id=${uploadId}&part_number=${partNumber}`, data, {
      headers: {
        'Content-Type': 'application/octet-stream',
      },
    });
    return response.data;
  },

  async completeUpload(uploadId: string, parts: number[]): Promise<MultipartCompleteResponse> {
    const response = await api.post(`/upload/complete?upload_id=${uploadId}`, { parts });
    return response.data;
  },

  async abortUpload(uploadId: string): Promise<MultipartAbortResponse> {
    const response = await api.delete(`/upload/abort?upload_id=${uploadId}`);
    return response.data;
  },

  async getUploadProgress(uploadId: string): Promise<MultipartProgressResponse> {
    const response = await api.get(`/upload/progress?upload_id=${uploadId}`);
    return response.data;
  },
};