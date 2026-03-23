import React, { useEffect, useState } from 'react';
import { User, UserRole } from '../../types/api';
import { usersService } from '../../services/users';
import { formatDate } from '../../utils/format';
import { UserRoleBadge } from './UserRoleBadge';
import { UserForm } from './UserForm';
import { Modal } from '../common/Modal';
import { Button } from '../common/Button';
import { Plus, Edit2, Trash2, UserCheck, UserX } from 'lucide-react';

export const UserList: React.FC = () => {
  const [users, setUsers] = useState<User[]>([]);
  const [loading, setLoading] = useState(true);
  const [showCreateModal, setShowCreateModal] = useState(false);
  const [editingUser, setEditingUser] = useState<User | null>(null);
  const [deletingUserId, setDeletingUserId] = useState<string | null>(null);

  useEffect(() => {
    loadUsers();
  }, []);

  const loadUsers = async () => {
    try {
      setLoading(true);
      const data = await usersService.listUsers();
      setUsers(data);
    } catch (error) {
      console.error('Error loading users:', error);
      alert('Ошибка при загрузке списка пользователей');
    } finally {
      setLoading(false);
    }
  };

  const handleCreateUser = async (username: string, email: string, role: UserRole) => {
    try {
      await usersService.createUser(username, email, role);
      setShowCreateModal(false);
      loadUsers();
      alert('Пользователь успешно создан');
    } catch (error) {
      console.error('Error creating user:', error);
      alert('Ошибка при создании пользователя');
    }
  };

  const handleUpdateRole = async (userId: string, role: UserRole) => {
    try {
      await usersService.updateUserRole(userId, role);
      loadUsers();
      alert('Роль пользователя успешно обновлена');
    } catch (error) {
      console.error('Error updating user role:', error);
      alert('Ошибка при обновлении роли');
    }
  };

  const handleActivateUser = async (userId: string) => {
    try {
      await usersService.activateUser(userId);
      loadUsers();
      alert('Пользователь активирован');
    } catch (error) {
      console.error('Error activating user:', error);
      alert('Ошибка при активации пользователя');
    }
  };

  const handleDeactivateUser = async (userId: string) => {
    try {
      await usersService.deactivateUser(userId);
      loadUsers();
      alert('Пользователь деактивирован');
    } catch (error) {
      console.error('Error deactivating user:', error);
      alert('Ошибка при деактивации пользователя');
    }
  };

  const handleDeleteUser = async (userId: string) => {
    if (!confirm('Вы уверены, что хотите удалить этого пользователя?')) {
      return;
    }

    try {
      setDeletingUserId(userId);
      await usersService.deleteUser(userId);
      loadUsers();
      alert('Пользователь успешно удален');
    } catch (error) {
      console.error('Error deleting user:', error);
      alert('Ошибка при удалении пользователя');
    } finally {
      setDeletingUserId(null);
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
          <h2 className="text-2xl font-bold text-gray-900">Пользователи</h2>
          <p className="text-gray-500 mt-1">Управление пользователями системы</p>
        </div>
        <Button onClick={() => setShowCreateModal(true)}>
          <Plus className="w-4 h-4 mr-2" />
          Добавить пользователя
        </Button>
      </div>

      <div className="bg-white shadow rounded-lg overflow-hidden">
        <table className="min-w-full divide-y divide-gray-200">
          <thead className="bg-gray-50">
            <tr>
              <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                Имя пользователя
              </th>
              <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                Email
              </th>
              <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                Роль
              </th>
              <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                Статус
              </th>
              <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                Последний вход
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
            {users.map((user) => (
              <tr key={user.user_id} className="hover:bg-gray-50">
                <td className="px-6 py-4 whitespace-nowrap">
                  <div className="text-sm font-medium text-gray-900">{user.username}</div>
                </td>
                <td className="px-6 py-4 whitespace-nowrap">
                  <div className="text-sm text-gray-500">{user.email}</div>
                </td>
                <td className="px-6 py-4 whitespace-nowrap">
                  <UserRoleBadge role={user.role} />
                </td>
                <td className="px-6 py-4 whitespace-nowrap">
                  <span className={`px-2 inline-flex text-xs leading-5 font-semibold rounded-full ${
                    user.is_active
                      ? 'bg-green-100 text-green-800'
                      : 'bg-red-100 text-red-800'
                  }`}>
                    {user.is_active ? 'Активен' : 'Неактивен'}
                  </span>
                </td>
                <td className="px-6 py-4 whitespace-nowrap">
                  <div className="text-sm text-gray-500">
                    {user.last_login ? formatDate(user.last_login) : 'Никогда'}
                  </div>
                </td>
                <td className="px-6 py-4 whitespace-nowrap">
                  <div className="text-sm text-gray-500">{formatDate(user.created_at)}</div>
                </td>
                <td className="px-6 py-4 whitespace-nowrap text-right text-sm font-medium">
                  <div className="flex justify-end space-x-2">
                    <button
                      onClick={() => setEditingUser(user)}
                      className="text-primary-600 hover:text-primary-900"
                      title="Редактировать"
                    >
                      <Edit2 className="w-4 h-4" />
                    </button>
                    {user.is_active ? (
                      <button
                        onClick={() => handleDeactivateUser(user.user_id)}
                        className="text-yellow-600 hover:text-yellow-900"
                        title="Деактивировать"
                      >
                        <UserX className="w-4 h-4" />
                      </button>
                    ) : (
                      <button
                        onClick={() => handleActivateUser(user.user_id)}
                        className="text-green-600 hover:text-green-900"
                        title="Активировать"
                      >
                        <UserCheck className="w-4 h-4" />
                      </button>
                    )}
                    <button
                      onClick={() => handleDeleteUser(user.user_id)}
                      className="text-red-600 hover:text-red-900"
                      disabled={deletingUserId === user.user_id}
                      title="Удалить"
                    >
                      {deletingUserId === user.user_id ? (
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

      {/* Create User Modal */}
      <Modal
        isOpen={showCreateModal}
        onClose={() => setShowCreateModal(false)}
        title="Создать пользователя"
      >
        <UserForm
          onSubmit={handleCreateUser}
          onCancel={() => setShowCreateModal(false)}
        />
      </Modal>

      {/* Edit User Modal */}
      <Modal
        isOpen={!!editingUser}
        onClose={() => setEditingUser(null)}
        title="Редактировать пользователя"
      >
        {editingUser && (
          <UserForm
            user={editingUser}
            onSubmit={(_, __, role) => handleUpdateRole(editingUser.user_id, role)}
            onCancel={() => setEditingUser(null)}
          />
        )}
      </Modal>
    </div>
  );
};