class S3Client {
    constructor(baseUrl = 'http://localhost:9000') {
        this.baseUrl = baseUrl;
        this.activeUploads = new Map();
    }

    async request(endpoint, options = {}) {
        try {
            const response = await fetch(`${this.baseUrl}${endpoint}`, {
                ...options,
                headers: {
                    'Access-Control-Allow-Origin': '*',
                    ...options.headers
                }
            });

            if (!response.ok) {
                const errorText = await response.text();
                throw new Error(`HTTP ${response.status}: ${errorText}`);
            }

            const contentType = response.headers.get('content-type');
            if (contentType && contentType.includes('application/json')) {
                return await response.json();
            }
            return await response.text();
        } catch (error) {
            console.error('Request failed:', error);
            throw error;
        }
    }

    async listFiles() {
        return await this.request('/list');
    }

    async uploadFile(filename, data) {
        return await this.request(`/${encodeURIComponent(filename)}`, {
            method: 'PUT',
            body: data
        });
    }

    async downloadFile(filename) {
        const response = await fetch(`${this.baseUrl}/${encodeURIComponent(filename)}`);
        if (!response.ok) {
            throw new Error(`Failed to download: ${response.status}`);
        }
        return await response.blob();
    }

    async deleteFile(filename) {
        return await this.request(`/${encodeURIComponent(filename)}`, {
            method: 'DELETE'
        });
    }

    async initiateMultipartUpload(filename) {
        return await this.request(`/upload/initiate?filename=${encodeURIComponent(filename)}`, {
            method: 'POST'
        });
    }

    async uploadPart(uploadId, partNumber, data) {
        return await this.request(
            `/upload/part?upload_id=${uploadId}&part_number=${partNumber}`,
            {
                method: 'PUT',
                body: data
            }
        );
    }

    async completeMultipartUpload(uploadId, partNumbers) {
        return await this.request(`/upload/complete?upload_id=${uploadId}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ parts: partNumbers })
        });
    }

    async abortMultipartUpload(uploadId) {
        return await this.request(`/upload/abort?upload_id=${uploadId}`, {
            method: 'DELETE'
        });
    }

    async getUploadProgress(uploadId) {
        return await this.request(`/upload/progress?upload_id=${uploadId}`);
    }
}

// Глобальные переменные
const client = new S3Client();
let files = [];
let selectedFiles = new Set();

// Инициализация
document.addEventListener('DOMContentLoaded', async () => {
    loadSettings();
    setupEventListeners();
    await checkServerStatus();
    await refreshFileList();
});

function loadSettings() {
    const savedUrl = localStorage.getItem('s3ServerUrl');
    if (savedUrl) {
        document.getElementById('serverUrl').value = savedUrl;
        client.baseUrl = savedUrl;
    }

    const savedMaxSize = localStorage.getItem('maxFileSize');
    if (savedMaxSize) {
        document.getElementById('maxFileSize').value = savedMaxSize;
    }
}

function saveSettings() {
    const serverUrl = document.getElementById('serverUrl').value;
    const maxFileSize = document.getElementById('maxFileSize').value;

    localStorage.setItem('s3ServerUrl', serverUrl);
    localStorage.setItem('maxFileSize', maxFileSize);

    client.baseUrl = serverUrl;

    showNotification('Настройки сохранены!', 'success');
}

function setupEventListeners() {
    // File input
    document.getElementById('fileInput').addEventListener('change', handleFileSelect);

    // Drop zone
    const dropZone = document.getElementById('dropZone');
    dropZone.addEventListener('click', () => document.getElementById('fileInput').click());
    dropZone.addEventListener('dragover', (e) => {
        e.preventDefault();
        dropZone.classList.add('drag-over');
    });
    dropZone.addEventListener('dragleave', () => dropZone.classList.remove('drag-over'));
    dropZone.addEventListener('drop', (e) => {
        e.preventDefault();
        dropZone.classList.remove('drag-over');
        handleDrop(e);
    });

    // Tab switching
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
            document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
            btn.classList.add('active');
            document.getElementById(`${btn.dataset.tab}-tab`).classList.add('active');
        });
    });

    // Search
    document.getElementById('searchInput').addEventListener('input', filterFiles);
}

async function checkServerStatus() {
    try {
        await client.listFiles();
        document.getElementById('serverStatus').innerHTML = '🟢 Online';
    } catch (error) {
        document.getElementById('serverStatus').innerHTML = '🔴 Offline';
        showNotification('Сервер недоступен!', 'error');
    }
}

async function refreshFileList() {
    try {
        const data = await client.listFiles();
        files = data.files || [];
        renderFileList();
        updateStats();
    } catch (error) {
        console.error('Failed to refresh file list:', error);
        showNotification('Ошибка при загрузке списка файлов', 'error');
    }
}

function renderFileList() {
    const fileList = document.getElementById('fileList');
    const searchQuery = document.getElementById('searchInput').value.toLowerCase();

    if (files.length === 0) {
        fileList.innerHTML = `
            <div class="empty-state">
                <div class="empty-icon">📭</div>
                <p>Нет файлов</p>
                <p class="empty-subtext">Загрузите первый файл</p>
            </div>
        `;
        return;
    }

    const filteredFiles = files.filter(file =>
        file.name.toLowerCase().includes(searchQuery)
    );

    if (filteredFiles.length === 0) {
        fileList.innerHTML = `
            <div class="empty-state">
                <div class="empty-icon">🔍</div>
                <p>Ничего не найдено</p>
            </div>
        `;
        return;
    }

    fileList.innerHTML = filteredFiles.map(file => {
        const icon = getFileIcon(file.name);
        const size = formatFileSize(file.size);
        const date = new Date(file.last_modified).toLocaleString('ru-RU');

        return `
            <div class="file-item" data-filename="${file.name}">
                <div class="file-checkbox">
                    <input type="checkbox" class="file-checkbox-input" 
                           data-filename="${file.name}" onchange="toggleFileSelection()">
                </div>
                <div class="file-name">
                    <span class="file-icon">${icon}</span>
                    ${escapeHtml(file.name)}
                </div>
                <div class="file-size">${size}</div>
                <div class="file-date">${date}</div>
                <div class="file-actions">
                    <button class="btn btn-secondary" onclick="downloadFile('${escapeHtml(file.name)}')">
                        📥
                    </button>
                    <button class="btn btn-danger" onclick="deleteFile('${escapeHtml(file.name)}')">
                        🗑️
                    </button>
                </div>
            </div>
        `;
    }).join('');

    updateDeleteButtonState();
}

function updateStats() {
    const totalFiles = files.length;
    const totalSize = files.reduce((sum, file) => sum + file.size, 0);

    document.getElementById('totalFiles').textContent = totalFiles;
    document.getElementById('totalSize').textContent = formatFileSize(totalSize);
    document.getElementById('storageInfo').textContent = `📦 ${totalFiles} файлов`;
}

function filterFiles() {
    renderFileList();
}

function getFileIcon(filename) {
    const ext = filename.split('.').pop().toLowerCase();
    const icons = {
        'txt': '📄', 'pdf': '📕', 'doc': '📘', 'docx': '📘',
        'jpg': '🖼️', 'jpeg': '🖼️', 'png': '🖼️', 'gif': '🖼️', 'svg': '🖼️',
        'mp4': '🎬', 'avi': '🎬', 'mkv': '🎬',
        'mp3': '🎵', 'wav': '🎵',
        'zip': '📦', 'rar': '📦', '7z': '📦', 'tar': '📦', 'gz': '📦',
        'exe': '⚙️', 'msi': '⚙️',
        'html': '🌐', 'css': '🎨', 'js': '📜', 'json': '📋'
    };
    return icons[ext] || '📄';
}

function toggleFileSelection() {
    selectedFiles.clear();
    document.querySelectorAll('.file-checkbox-input:checked').forEach(checkbox => {
        selectedFiles.add(checkbox.dataset.filename);
    });
    updateDeleteButtonState();
}

function updateDeleteButtonState() {
    const deleteBtn = document.getElementById('deleteBtn');
    deleteBtn.disabled = selectedFiles.size === 0;
}

async function deleteSelectedFiles() {
    if (selectedFiles.size === 0) return;

    if (!confirm(`Удалить ${selectedFiles.size} файл(ов)?`)) {
        return;
    }

    let successCount = 0;
    let errorCount = 0;

    for (const filename of selectedFiles) {
        try {
            await client.deleteFile(filename);
            successCount++;
        } catch (error) {
            console.error(`Failed to delete ${filename}:`, error);
            errorCount++;
        }
    }

    selectedFiles.clear();
    await refreshFileList();

    if (errorCount === 0) {
        showNotification(`Удалено ${successCount} файл(ов)`, 'success');
    } else {
        showNotification(`Удалено ${successCount}, ошибок ${errorCount}`, 'warning');
    }
}

async function deleteFile(filename) {
    if (!confirm(`Удалить файл "${filename}"?`)) {
        return;
    }

    try {
        await client.deleteFile(filename);
        await refreshFileList();
        showNotification(`Файл "${filename}" удален`, 'success');
    } catch (error) {
        console.error('Delete failed:', error);
        showNotification(`Ошибка при удалении файла: ${error.message}`, 'error');
    }
}

async function downloadFile(filename) {
    try {
        const blob = await client.downloadFile(filename);
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        window.URL.revokeObjectURL(url);
        document.body.removeChild(a);
    } catch (error) {
        console.error('Download failed:', error);
        showNotification(`Ошибка при скачивании файла: ${error.message}`, 'error');
    }
}

function handleDrop(e) {
    const files = Array.from(e.dataTransfer.files);
    uploadFiles(files);
}

function handleFileSelect(e) {
    const files = Array.from(e.target.files);
    uploadFiles(files);
    e.target.value = '';
}

async function uploadFiles(fileList) {
    if (fileList.length === 0) return;

    const useMultipart = document.getElementById('useMultipart').checked;
    const chunkSizeMB = parseInt(document.getElementById('chunkSize').value);
    const chunkSize = chunkSizeMB * 1024 * 1024;
    const maxFileSize = parseInt(document.getElementById('maxFileSize').value) * 1024 * 1024;

    for (const file of fileList) {
        try {
            const shouldUseMultipart = useMultipart || file.size > maxFileSize;

            if (shouldUseMultipart) {
                await uploadFileMultipart(file, chunkSize);
            } else {
                await uploadFileSimple(file);
            }
        } catch (error) {
            console.error(`Upload failed for ${file.name}:`, error);
            showNotification(`Ошибка загрузки ${file.name}: ${error.message}`, 'error');
        }
    }

    await refreshFileList();
}

async function uploadFileSimple(file) {
    const uploadId = `simple-${Date.now()}`;
    addUploadToList(uploadId, file.name, 0, file.size);

    try {
        const arrayBuffer = await file.arrayBuffer();
        const data = new Uint8Array(arrayBuffer);

        await client.uploadFile(file.name, data);

        updateUploadProgress(uploadId, file.size, file.size);
        removeUploadFromList(uploadId);

        showNotification(`Файл "${file.name}" загружен`, 'success');
    } catch (error) {
        removeUploadFromList(uploadId);
        throw error;
    }
}

async function uploadFileMultipart(file, chunkSize) {
    const initResponse = await client.initiateMultipartUpload(file.name);
    const uploadId = initResponse.upload_id;

    addUploadToList(uploadId, file.name, 0, file.size);

    try {
        const totalChunks = Math.ceil(file.size / chunkSize);
        const uploadedParts = [];
        let uploadedBytes = 0;

        for (let i = 0; i < totalChunks; i++) {
            const start = i * chunkSize;
            const end = Math.min(start + chunkSize, file.size);
            const chunk = file.slice(start, end);

            const arrayBuffer = await chunk.arrayBuffer();
            const data = new Uint8Array(arrayBuffer);

            await client.uploadPart(uploadId, i + 1, data);

            uploadedParts.push(i + 1);
            uploadedBytes += data.length;

            updateUploadProgress(uploadId, uploadedBytes, file.size);
        }

        await client.completeMultipartUpload(uploadId, uploadedParts);

        removeUploadFromList(uploadId);
        showNotification(`Файл "${file.name}" загружен (по частям)`, 'success');
    } catch (error) {
        try {
            await client.abortMultipartUpload(uploadId);
        } catch (e) {
            console.error('Failed to abort upload:', e);
        }
        removeUploadFromList(uploadId);
        throw error;
    }
}

function addUploadToList(uploadId, filename, uploaded, total) {
    const uploadList = document.getElementById('uploadList');
    const isEmpty = uploadList.querySelector('.empty-state');

    if (isEmpty) {
        uploadList.innerHTML = '';
    }

    const uploadItem = document.createElement('div');
    uploadItem.className = 'upload-item';
    uploadItem.id = `upload-${uploadId}`;
    uploadItem.innerHTML = `
        <div class="upload-header">
            <div class="upload-filename">${escapeHtml(filename)}</div>
            <div class="upload-status" id="status-${uploadId}">Загрузка...</div>
        </div>
        <div class="progress-container">
            <div class="progress-bar" id="progress-${uploadId}" style="width: 0%"></div>
        </div>
        <div class="progress-info">
            <span id="progress-text-${uploadId}">0%</span>
            <span id="size-text-${uploadId}">${formatFileSize(uploaded)} / ${formatFileSize(total)}</span>
        </div>
    `;

    uploadList.insertBefore(uploadItem, uploadList.firstChild);
}

function updateUploadProgress(uploadId, uploaded, total) {
    const progress = Math.min(100, Math.round((uploaded / total) * 100));
    const progressBar = document.getElementById(`progress-${uploadId}`);
    const progressText = document.getElementById(`progress-text-${uploadId}`);
    const sizeText = document.getElementById(`size-text-${uploadId}`);

    if (progressBar) progressBar.style.width = `${progress}%`;
    if (progressText) progressText.textContent = `${progress}%`;
    if (sizeText) sizeText.textContent = `${formatFileSize(uploaded)} / ${formatFileSize(total)}`;
}

function removeUploadFromList(uploadId) {
    const uploadItem = document.getElementById(`upload-${uploadId}`);
    if (uploadItem) {
        uploadItem.remove();
    }

    const uploadList = document.getElementById('uploadList');
    if (uploadList.children.length === 0) {
        uploadList.innerHTML = `
            <div class="empty-state">
                <div class="empty-icon">⬆️</div>
                <p>Нет активных загрузок</p>
            </div>
        `;
    }
}

function showNotification(message, type = 'info') {
    const notification = document.getElementById('notification');
    notification.textContent = message;
    notification.className = `notification ${type} show`;

    setTimeout(() => {
        notification.classList.remove('show');
    }, 3000);
}

function formatFileSize(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}