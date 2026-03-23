import React, { useEffect, useState } from 'react';
import { policiesService } from '../../services/policies';
import { formatDate } from '../../utils/format';
import { PolicyForm } from './PolicyForm';
import { Modal } from '../common/Modal';
import { Button } from '../common/Button';
import { Plus, Edit2, Trash2, FileLock2, ShieldCheck } from 'lucide-react';
import { getPermissionBadgeColor } from '../../utils/format';
import { AccessPolicy, Permission } from '../../types/api';

export const PolicyList: React.FC = () => {
  const [policies, setPolicies] = useState<AccessPolicy[]>([]);
  const [loading, setLoading] = useState(true);
  const [showCreateModal, setShowCreateModal] = useState(false);
  const [editingPolicy, setEditingPolicy] = useState<AccessPolicy | null>(null);
  const [deletingPolicyId, setDeletingPolicyId] = useState<string | null>(null);

  useEffect(() => {
    loadPolicies();
  }, []);

  const loadPolicies = async () => {
    try {
      setLoading(true);
      const data = await policiesService.listPolicies();
      setPolicies(data);
    } catch (error) {
      console.error('Error loading policies:', error);
      alert('Ошибка при загрузке списка политик');
    } finally {
      setLoading(false);
    }
  };

  const handleCreatePolicy = async (
    name: string,
    description: string,
    permissions: Permission[]
  ) => {
    try {
      await policiesService.createPolicy(name, description, permissions);
      setShowCreateModal(false);
      loadPolicies();
      alert('Политика успешно создана');
    } catch (error) {
      console.error('Error creating policy:', error);
      alert('Ошибка при создании политики');
    }
  };

  const handleDeletePolicy = async (policyId: string) => {
    if (!confirm('Вы уверены, что хотите удалить эту политику?')) {
      return;
    }

    try {
      setDeletingPolicyId(policyId);
      await policiesService.deletePolicy(policyId);
      loadPolicies();
      alert('Политика успешно удалена');
    } catch (error) {
      console.error('Error deleting policy:', error);
      alert('Ошибка при удалении политики');
    } finally {
      setDeletingPolicyId(null);
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
          <h2 className="text-2xl font-bold text-gray-900">Политики доступа</h2>
          <p className="text-gray-500 mt-1">Управление политиками доступа к ресурсам</p>
        </div>
        <Button onClick={() => setShowCreateModal(true)}>
          <Plus className="w-4 h-4 mr-2" />
          Создать политику
        </Button>
      </div>

      <div className="grid gap-6">
        {policies.map((policy) => (
          <div
            key={policy.policy_id}
            className="bg-white shadow rounded-lg overflow-hidden hover:shadow-md transition-shadow"
          >
            <div className="p-6">
              <div className="flex justify-between items-start">
                <div className="flex-1">
                  <div className="flex items-center space-x-2">
                    <ShieldCheck className="w-5 h-5 text-primary-600" />
                    <h3 className="text-lg font-semibold text-gray-900">{policy.name}</h3>
                  </div>
                  <p className="text-gray-600 mt-1 text-sm">{policy.description}</p>
                  
                  <div className="mt-4">
                    <h4 className="text-sm font-medium text-gray-700 mb-2">Разрешения:</h4>
                    <div className="flex flex-wrap gap-2">
                      {policy.permissions.map((perm, idx) => (
                        <span
                          key={idx}
                          className={`px-2 py-1 rounded text-xs font-medium ${
                            getPermissionBadgeColor(perm.type)
                          }`}
                        >
                          {perm.type} {perm.allow ? '✓' : '✗'} - {perm.resource_pattern}
                        </span>
                      ))}
                    </div>
                  </div>
                </div>
                
                <div className="flex space-x-2 ml-4">
                  <button
                    onClick={() => setEditingPolicy(policy)}
                    className="p-2 text-gray-400 hover:text-primary-600 rounded hover:bg-gray-100"
                    title="Редактировать"
                  >
                    <Edit2 className="w-4 h-4" />
                  </button>
                  <button
                    onClick={() => handleDeletePolicy(policy.policy_id)}
                    className="p-2 text-gray-400 hover:text-red-600 rounded hover:bg-gray-100"
                    disabled={deletingPolicyId === policy.policy_id}
                    title="Удалить"
                  >
                    {deletingPolicyId === policy.policy_id ? (
                      <div className="animate-spin rounded-full h-4 w-4 border-b-2 border-red-600"></div>
                    ) : (
                      <Trash2 className="w-4 h-4" />
                    )}
                  </button>
                </div>
              </div>

              <div className="mt-4 pt-4 border-t border-gray-200 text-sm text-gray-500">
                Создано: {formatDate(policy.created_at)}
              </div>
            </div>
          </div>
        ))}
      </div>

      {policies.length === 0 && (
        <div className="text-center py-12 bg-white rounded-lg shadow">
          <FileLock2 className="w-16 h-16 mx-auto text-gray-400" />
          <h3 className="mt-2 text-sm font-medium text-gray-900">Нет политик доступа</h3>
          <p className="mt-1 text-sm text-gray-500">
            Создайте первую политику для управления доступом к ресурсам
          </p>
          <div className="mt-6">
            <Button onClick={() => setShowCreateModal(true)}>
              <Plus className="w-4 h-4 mr-2" />
              Создать политику
            </Button>
          </div>
        </div>
      )}

      <Modal
        isOpen={showCreateModal || !!editingPolicy}
        onClose={() => {
          setShowCreateModal(false);
          setEditingPolicy(null);
        }}
        title={editingPolicy ? 'Редактировать политику' : 'Создать политику'}
        size="lg"
      >
        <PolicyForm
          policy={editingPolicy || undefined}
          onSubmit={handleCreatePolicy}
          onCancel={() => {
            setShowCreateModal(false);
            setEditingPolicy(null);
          }}
        />
      </Modal>
    </div>
  );
};