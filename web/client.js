// Пример загрузки большого файла по частям
class S3Client {
    constructor(baseUrl) {
        this.baseUrl = baseUrl;
    }

    async initiateUpload(filename) {
        const response = await fetch(
            `${this.baseUrl}/upload/initiate?filename=${encodeURIComponent(filename)}`,
            { method: 'POST' }
        );
        return await response.json();
    }

    async uploadPart(uploadId, partNumber, data) {
        const response = await fetch(
            `${this.baseUrl}/upload/part?upload_id=${uploadId}&part_number=${partNumber}`,
            {
                method: 'PUT',
                body: data
            }
        );
        return await response.json();
    }

    async completeUpload(uploadId, partNumbers) {
        const response = await fetch(
            `${this.baseUrl}/upload/complete?upload_id=${uploadId}`,
            {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ parts: partNumbers })
            }
        );
        return await response.json();
    }

    async abortUpload(uploadId) {
        const response = await fetch(
            `${this.baseUrl}/upload/abort?upload_id=${uploadId}`,
            { method: 'DELETE' }
        );
        return await response.json();
    }

    async getProgress(uploadId) {
        const response = await fetch(
            `${this.baseUrl}/upload/progress?upload_id=${uploadId}`
        );
        return await response.json();
    }

    async uploadFile(file, chunkSize = 5 * 1024 * 1024) { // 5MB chunks
        try {
            // Инициируем загрузку
            const initResponse = await this.initiateUpload(file.name);
            const uploadId = initResponse.upload_id;
            
            const totalChunks = Math.ceil(file.size / chunkSize);
            const uploadedParts = [];
            
            console.log(`Uploading ${file.name} in ${totalChunks} parts...`);
            
            // Загружаем части
            for (let i = 0; i < totalChunks; i++) {
                const start = i * chunkSize;
                const end = Math.min(start + chunkSize, file.size);
                const chunk = file.slice(start, end);
                
                const arrayBuffer = await chunk.arrayBuffer();
                const data = new Uint8Array(arrayBuffer);
                
                await this.uploadPart(uploadId, i + 1, data);
                uploadedParts.push(i + 1);
                
                const progress = ((i + 1) / totalChunks * 100).toFixed(2);
                console.log(`Progress: ${progress}% (${i + 1}/${totalChunks})`);
            }
            
            // Завершаем загрузку
            await this.completeUpload(uploadId, uploadedParts);
            
            console.log('Upload completed successfully!');
            return true;
            
        } catch (error) {
            console.error('Upload failed:', error);
            throw error;
        }
    }
}

// Использование
const client = new S3Client('http://localhost:9000');

// HTML input для файла
document.getElementById('fileInput').addEventListener('change', async (e) => {
    const file = e.target.files[0];
    if (file) {
        await client.uploadFile(file);
    }
});