import React, { useEffect, useState } from 'react';
import { FileMetadata } from '../../types/api';
import { filesService } from '../../services/files';
import { formatBytes, formatDate } from '../../utils/format';
import { Download, Trash2, Eye } from 'lucide-react';
import { Button } from '../common/Button';

interface FileListProps {
  onFileSelect?: (file: FileMetadata) => void;
}

export const FileList: React.FC<FileListProps> = ({ onFileSelect }) => {
  const [files, setFiles] = useState<FileMetadata[]>([]);
  const [loading, setLoading] = useState(true);
  const [deletingFile, setDeletingFile] = useState<string | null>(null);

  useEffect(() => {
    loadFiles();
  }, []);

  const loadFiles = async () => {
    try {
      setLoading(true);
      const data = await filesService.listFiles();
      setFiles(data.files || []);
    } catch (error) {
      console.error('Error loading files:', error);
    } finally {
      setLoading(false);
    }
  };

  const handleDownload = async (filename: string) => {
    try {
      const blob = await filesService.downloadFile(filename);
      const url = window.URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = filename;
      document.body.appendChild(a);
      a.click();
      window.URL.revokeObjectURL(url);
      document.body.removeChild(a);
    } catch (error) {
      console.error('Error downloading file:', error);
    }
  };

  const handleDelete = async (filename: string) => {
    if (!confirm(`Вы уверены, что хотите удалить файл "${filename}"?`)) {
      return;
    }

    try {
      setDeletingFile(filename);
      await filesService.deleteFile(filename);
      setFiles(files.filter(f => f.name !== filename));
    } catch (error) {
      console.error('Error deleting file:', error);
      alert('Ошибка при удалении файла');
    } finally {
      setDeletingFile(null);
    }
  };

  if (loading) {
    return (
      <div className="flex justify-center items-center h-64">
        <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-primary-600"></div>
      </div>
    );
  }

  if (files.length === 0) {
    return (
      <div className="text-center py-12">
        <p className="text-gray-500">Нет файлов</p>
      </div>
    );
  }

  return (
    <div className="bg-white shadow rounded-lg overflow-hidden">
      <table className="min-w-full divide-y divide-gray-200">
        <thead className="bg-gray-50">
          <tr>
            <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
              Имя файла
            </th>
            <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
              Размер
            </th>
            <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
              Дата изменения
            </th>
            <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
              ETag
            </th>
            <th className="px-6 py-3 text-right text-xs font-medium text-gray-500 uppercase tracking-wider">
              Действия
            </th>
          </tr>
        </thead>
        <tbody className="bg-white divide-y divide-gray-200">
          {files.map((file) => (
            <tr key={file.name} className="hover:bg-gray-50">
              <td className="px-6 py-4 whitespace-nowrap">
                <div className="text-sm font-medium text-gray-900">{file.name}</div>
              </td>
              <td className="px-6 py-4 whitespace-nowrap">
                <div className="text-sm text-gray-500">{formatBytes(file.size)}</div>
              </td>
              <td className="px-6 py-4 whitespace-nowrap">
                <div className="text-sm text-gray-500">{formatDate(file.last_modified)}</div>
              </td>
              <td className="px-6 py-4 whitespace-nowrap">
                <div className="text-xs text-gray-400 truncate max-w-xs">{file.etag}</div>
              </td>
              <td className="px-6 py-4 whitespace-nowrap text-right text-sm font-medium">
                <div className="flex justify-end space-x-2">
                  <Button
                    variant="ghost"
                    size="sm"
                    onClick={() => handleDownload(file.name)}
                  >
                    <Download className="w-4 h-4" />
                  </Button>
                  <Button
                    variant="ghost"
                    size="sm"
                    onClick={() => onFileSelect?.(file)}
                  >
                    <Eye className="w-4 h-4" />
                  </Button>
                  <Button
                    variant="ghost"
                    size="sm"
                    onClick={() => handleDelete(file.name)}
                    disabled={deletingFile === file.name}
                  >
                    <Trash2 className="w-4 h-4 text-red-600" />
                  </Button>
                </div>
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
};