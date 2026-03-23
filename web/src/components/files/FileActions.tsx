import React from 'react';
import { FileMetadata } from '../../types/api';
import { filesService } from '../../services/files';
import { Download, Trash2, Share2, Lock, Unlock, Link } from 'lucide-react'; // Убраны неиспользуемые импорты
import { Button } from '../common/Button';

interface FileActionsProps {
  file: FileMetadata;
  onDownload?: (file: FileMetadata) => void;
  onDelete?: (file: FileMetadata) => void;
  onShare?: (file: FileMetadata) => void;
  onMakePublic?: (file: FileMetadata) => void;
  onMakePrivate?: (file: FileMetadata) => void;
}

export const FileActions: React.FC<FileActionsProps> = ({
  file,
  onDownload,
  onDelete,
  onShare,
  onMakePublic,
  onMakePrivate,
}) => {
  const handleDownload = async () => {
    if (onDownload) {
      onDownload(file);
      return;
    }

    try {
      const blob = await filesService.downloadFile(file.name);
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = file.name;
      document.body.appendChild(a);
      a.click();
      URL.revokeObjectURL(url);
      document.body.removeChild(a);
    } catch (error) {
      console.error('Error downloading file:', error);
      alert('Ошибка при скачивании файла');
    }
  };

  const handleDelete = () => {
    if (onDelete) {
      onDelete(file);
      return;
    }

    if (confirm(`Вы уверены, что хотите удалить файл "${file.name}"?`)) {
      filesService.deleteFile(file.name)
        .then(() => {
          alert('Файл успешно удален');
          window.location.reload();
        })
        .catch((error) => {
          console.error('Error deleting file:', error);
          alert('Ошибка при удалении файла');
        });
    }
  };

  const handleCopyLink = async () => {
    try {
      const url = `${window.location.origin}/${file.name}`;
      await navigator.clipboard.writeText(url);
      alert('Ссылка скопирована в буфер обмена');
    } catch (error) {
      console.error('Error copying link:', error);
    }
  };

  return (
    <div className="flex space-x-1">
      <Button
        variant="ghost"
        size="sm"
        onClick={handleDownload}
        title="Скачать"
      >
        <Download className="w-4 h-4" />
      </Button>

      {onShare && (
        <Button
          variant="ghost"
          size="sm"
          onClick={() => onShare(file)}
          title="Поделиться"
        >
          <Share2 className="w-4 h-4" />
        </Button>
      )}

      {onMakePublic && (
        <Button
          variant="ghost"
          size="sm"
          onClick={() => onMakePublic(file)}
          title="Сделать публичным"
        >
          <Unlock className="w-4 h-4" />
        </Button>
      )}

      {onMakePrivate && (
        <Button
          variant="ghost"
          size="sm"
          onClick={() => onMakePrivate(file)}
          title="Сделать приватным"
        >
          <Lock className="w-4 h-4" />
        </Button>
      )}

      <Button
        variant="ghost"
        size="sm"
        onClick={handleCopyLink}
        title="Копировать ссылку"
      >
        <Link className="w-4 h-4" />
      </Button>

      <Button
        variant="ghost"
        size="sm"
        onClick={handleDelete}
        title="Удалить"
        className="text-red-600 hover:text-red-700"
      >
        <Trash2 className="w-4 h-4" />
      </Button>
    </div>
  );
};