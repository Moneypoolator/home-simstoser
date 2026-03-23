import React, { useEffect, useState } from 'react';
import { AccessKey } from '../../types/api';
import { keysService } from '../../services/keys';
import { formatDate } from '../../utils/format';
import { Modal } from '../common/Modal';
import { AccessKeyForm } from './AccessKeyForm';
import { Button } from '../common/Button';
import { Plus, Copy, Eye, EyeOff, Trash2, KeyRound, Lock, Unlock } from 'lucide-react';

export const AccessKeyList: React.FC = () => {
  const [keys, setKeys] = useState<AccessKey[]>([]);
  const [loading, setLoading] = useState(true);
  const [showCreateModal, setShowCreateModal] = useState(false);
  const [visibleSecrets, setVisibleSecrets] = useState<Set<string>>(new Set());
  const [deletingKeyId, setDeletingKeyId] = useState<string | null>(null);
  const [copiedKeyId, setCopiedKeyId] = useState<string | null>(null);

  useEffect(() => {
    loadKeys();
  }, []);

  const loadKeys = async () => {
    try {
      setLoading(true);
      const data = await keysService.listKeys();
      setKeys(data);
    } catch (error) {
      console.error('Error loading keys:', error);
      alert('Ошибка при загрузке списка ключей');
    } finally {
      setLoading(false);
    }
  };

  const handleCreateKey = async (username: string) => {
    try {
      const newKey = await keysService.createKey(username);
      setKeys([...keys, newKey]);
      setShowCreateModal(false);
      
      // Показываем секретный ключ
      const newVisible = new Set(visibleSecrets);
      newVisible.add(newKey.access_key_id);
      setVisibleSecrets(newVisible);
      
      alert('Ключ доступа успешно создан! Скопируйте секретный ключ - он больше не будет показан.');
    } catch (error) {
      console.error('Error creating key:', error);
      alert('Ошибка при создании ключа');
    }
  };

  const handleDeleteKey = async (accessKeyId: string) => {
    if (!confirm('Вы уверены, что хотите удалить этот ключ доступа?')) {
      return;
    }

    try {
      setDeletingKeyId(accessKeyId);
      await keysService.deleteKey(accessKeyId);
      setKeys(keys.filter(k => k.access_key_id !== accessKeyId));
      alert('Ключ успешно удален');
    } catch (error) {
      console.error('Error deleting key:', error);
      alert('Ошибка при удалении ключа');
    } finally {
      setDeletingKeyId(null);
    }
  };

  const handleToggleVisibility = (accessKeyId: string) => {
    const newVisible = new Set(visibleSecrets);
    if (newVisible.has(accessKeyId)) {
      newVisible.delete(accessKeyId);
    } else {
      newVisible.add(accessKeyId);
    }
    setVisibleSecrets(newVisible);
  };

  const handleCopySecret = async (accessKeyId: string, secretKey: string) => {
    try {
      await navigator.clipboard.writeText(secretKey);
      setCopiedKeyId(accessKeyId);
      setTimeout(() => setCopiedKeyId(null), 2000);
    } catch (error) {
      console.error('Error copying to clipboard:', error);
    }
  };

  const handleActivateKey = async (accessKeyId: string) => {
    try {
      await keysService.activateKey(accessKeyId);
      loadKeys();
      alert('Ключ активирован');
    } catch (error) {
      console.error('Error activating key:', error);
      alert('Ошибка при активации ключа');
    }
  };

  const handleDeactivateKey = async (accessKeyId: string) => {
    try {
      await keysService.deactivateKey(accessKeyId);
      loadKeys();
      alert('Ключ деактивирован');
    } catch (error) {
      console.error('Error deactivating key:', error);
      alert('Ошибка при деактивации ключа');
    }
  };

  if (loading) {
    return (
      <div className="flex justify-center items-center h-64">
        <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-primary-600"></div>
      </div>
    );
  }

  return (
    <div className="space-y-6">
      <div className="flex justify-between items-center">
        <div>
          <h2 className="text-2xl font-bold text-gray-900">Ключи доступа</h2>
          <p className="text-gray-500 mt-1">Управление ключами доступа API</p>
        </div>
        <Button onClick={() => setShowCreateModal(true)}>
          <Plus className="w-4 h-4 mr-2" />
          Создать ключ
        </Button>
      </div>

      <div className="bg-white shadow rounded-lg overflow-hidden">
        <table className="min-w-full divide-y divide-gray-200">
          <thead className="bg-gray-50">
            <tr>
              <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                Access Key ID
              </th>
              <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                Пользователь
              </th>
              <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                Секретный ключ
              </th>
              <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                Статус
              </th>
              <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                Создан
              </th>
              <th className="px-6 py-3 text-right text-xs font-medium text-gray-500 uppercase tracking-wider">
                Действия
              </th>
            </tr>
          </thead>
          <tbody className="bg-white divide-y divide-gray-200">
            {keys.map((key) => (
              <tr key={key.access_key_id} className="hover:bg-gray-50">
                <td className="px-6 py-4 whitespace-nowrap font-mono text-sm">
                  {key.access_key_id}
                </td>
                <td className="px-6 py-4 whitespace-nowrap">
                  <div className="text-sm text-gray-900">{key.user_name}</div>
                </td>
                <td className="px-6 py-4 whitespace-nowrap">
                  <div className="flex items-center space-x-2">
                    {visibleSecrets.has(key.access_key_id) && key.secret_access_key ? (
                      <span className="font-mono text-sm bg-gray-100 px-2 py-1 rounded">
                        {key.secret_access_key}
                      </span>
                    ) : (
                      <span className="text-gray-400 text-sm">••••••••••••••••</span>
                    )}
                    <button
                      onClick={() => handleToggleVisibility(key.access_key_id)}
                      className="text-gray-400 hover:text-gray-600"
                      title={visibleSecrets.has(key.access_key_id) ? 'Скрыть' : 'Показать'}
                    >
                      {visibleSecrets.has(key.access_key_id) ? (
                        <EyeOff className="w-4 h-4" />
                      ) : (
                        <Eye className="w-4 h-4" />
                      )}
                    </button>
                    {visibleSecrets.has(key.access_key_id) && key.secret_access_key && (
                      <button
                        onClick={() => handleCopySecret(key.access_key_id, key.secret_access_key || '')}
                        className="text-gray-400 hover:text-gray-600"
                        title="Копировать"
                      >
                        {copiedKeyId === key.access_key_id ? (
                          <span className="text-green-600 text-xs">Скопировано!</span>
                        ) : (
                          <Copy className="w-4 h-4" />
                        )}
                      </button>
                    )}
                  </div>
                </td>
                <td className="px-6 py-4 whitespace-nowrap">
                  <span className={`px-2 inline-flex text-xs leading-5 font-semibold rounded-full ${
                    key.is_active
                      ? 'bg-green-100 text-green-800'
                      : 'bg-red-100 text-red-800'
                  }`}>
                    {key.is_active ? 'Активен' : 'Неактивен'}
                  </span>
                </td>
                <td className="px-6 py-4 whitespace-nowrap">
                  <div className="text-sm text-gray-500">{formatDate(key.created_at)}</div>
                </td>
                <td className="px-6 py-4 whitespace-nowrap text-right text-sm font-medium">
                  <div className="flex justify-end space-x-2">
                    {key.is_active ? (
                      <button
                        onClick={() => handleDeactivateKey(key.access_key_id)}
                        className="text-yellow-600 hover:text-yellow-900"
                        title="Деактивировать"
                      >
                        <Lock className="w-4 h-4" />
                      </button>
                    ) : (
                      <button
                        onClick={() => handleActivateKey(key.access_key_id)}
                        className="text-green-600 hover:text-green-900"
                        title="Активировать"
                      >
                        <Unlock className="w-4 h-4" />
                      </button>
                    )}
                    <button
                      onClick={() => handleDeleteKey(key.access_key_id)}
                      className="text-red-600 hover:text-red-900"
                      disabled={deletingKeyId === key.access_key_id}
                      title="Удалить"
                    >
                      {deletingKeyId === key.access_key_id ? (
                        <div className="animate-spin rounded-full h-4 w-4 border-b-2 border-red-600"></div>
                      ) : (
                        <Trash2 className="w-4 h-4" />
                      )}
                    </button>
                  </div>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      {keys.length === 0 && (
        <div className="text-center py-12 bg-white rounded-lg shadow">
          <KeyRound className="w-16 h-16 mx-auto text-gray-400" />
          <h3 className="mt-2 text-sm font-medium text-gray-900">Нет ключей доступа</h3>
          <p className="mt-1 text-sm text-gray-500">
            Создайте первый ключ доступа для работы с API
          </p>
          <div className="mt-6">
            <Button onClick={() => setShowCreateModal(true)}>
              <Plus className="w-4 h-4 mr-2" />
              Создать ключ
            </Button>
          </div>
        </div>
      )}

      <Modal
        isOpen={showCreateModal}
        onClose={() => setShowCreateModal(false)}
        title="Создать ключ доступа"
      >
        <AccessKeyForm onSubmit={handleCreateKey} onCancel={() => setShowCreateModal(false)} />
      </Modal>
    </div>
  );
};