# Home S3 Storage

A lightweight, S3-compatible file storage server for personal use with a modern web interface. Built with C++20 for high performance and React for an intuitive user experience.

## 1. Product Description

Home S3 Storage is a self-hosted file storage solution that implements basic Amazon S3 API compatibility. It's designed for personal use, home labs, or small teams who need a simple yet powerful file storage system with S3-like functionality.

### Key Features

- **S3 API Compatibility**: Basic S3 operations (PUT, GET, DELETE, LIST) with multipart and stream upload support
- **Modern Web Interface**: React-based dashboard for file management with drag-and-drop uploads
- **High Performance**: Built with C++20 and Boost.Beast for efficient asynchronous I/O
- **Thread-Safe Operations**: Concurrent file access with proper locking mechanisms
- **Security Features**: Path traversal protection, AWS Signature Version 4 authentication, role-based authorization (RBAC), rate limiting, and DDoS protection
- **REST API**: Full RESTful API for programmatic access and integration
- **Multipart Uploads**: Support for large file uploads in chunks with progress tracking
- **Stream Uploads**: Incremental file writing for efficient large file handling
- **User Management**: Role-based access control with configurable permissions
- **File Preview**: Built-in preview for common file types (images, PDFs, text files)
- **SSL/TLS Support**: Secure HTTPS connections with configurable certificates

### Architecture

The application consists of several core components:

1. **Backend Server** (C++): HTTP/HTTPS server built on Boost.Beast handling S3 API requests with asynchronous I/O
2. **File Manager**: Thread-safe file operations with multipart and stream upload support, path traversal protection
3. **Authenticator**: AWS Signature Version 4 authentication for secure API access
4. **Authorizer**: Role-based access control (RBAC) with user and policy management
5. **Request Handler**: Processes HTTP requests, validates authentication/authorization, and routes to appropriate handlers
6. **Web Frontend** (React/TypeScript): Modern UI for file management, user administration, and system monitoring

## 2. Environment Installation Description

### Prerequisites

#### System Requirements

- Linux, macOS, or Windows with WSL2
- 2GB RAM minimum, 4GB recommended
- 1GB free disk space for the application + storage for your files

#### Dependencies

**Backend (C++ Server):**

- C++20 compatible compiler (GCC 11+ or Clang 13+)
- CMake 3.15+
- Boost 1.75+ (system component)
- OpenSSL 3.0+
- nlohmann/json library
- Google Logging (glog)

**Frontend (Web UI):**

- Node.js 18+ and npm/yarn
- TypeScript 5.7+

#### Installation Steps

1. **Install System Dependencies**

   **Ubuntu/Debian:**

   ```bash
   sudo apt update
   sudo apt install -y build-essential cmake libboost-system-dev libssl-dev nlohmann-json3-dev libgoogle-glog-dev
   ```

   **Fedora/RHEL:**

   ```bash
   sudo dnf install -y gcc-c++ cmake boost-devel openssl-devel nlohmann-json-devel glog-devel
   ```

   **macOS (Homebrew):**

   ```bash
   brew install cmake boost openssl nlohmann-json glog
   ```

2. **Install Node.js and npm**

   ```bash
   # Using Node Version Manager (recommended)
   curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.0/install.sh | bash
   nvm install 18
   nvm use 18
   ```

3. **Clone the Repository**

   ```bash
   git clone https://github.com/Moneypoolator/home-simstoser.git
   cd home-s3-storage
   ```

## 3. How to Build Application

### Backend Server Build

1. **Create Build Directory**

   ```bash
   mkdir -p build && cd build
   ```

2. **Configure with CMake**

   ```bash
   cmake .. -DCMAKE_BUILD_TYPE=Release
   ```

3. **Compile the Server**

   ```bash
   make -j$(nproc)
   ```

   The compiled binary will be available at `./s3_server`

### Frontend Web UI Build

1. **Navigate to Web Directory**

   ```bash
   cd ../web
   ```

2. **Install Dependencies**

   ```bash
   npm install
   ```

3. **Build for Production**

   ```bash
   npm run build
   ```

   The built frontend will be in `web/dist/`

### Combined Build Script

You can also use the provided build script:

```bash
chmod +x build-run.sh
./build-run.sh
```

This script will:

1. Build the frontend web UI (`npm run build` in `web/` directory)
2. Build the backend C++ server (creates `build/` directory, runs CMake and make)
3. Copy the built frontend to the appropriate location
4. Start the server with default settings (port 9000, storage in `./storage`, no authentication)

**Note:** The script assumes you have all dependencies installed. It may require `sudo` privileges for package installation on some systems.

## 4. How to Run Application

### Running the Server

#### Basic Startup

```bash
cd build
./s3_server
```

This starts the server with default settings:

- Address: `0.0.0.0` (all interfaces)
- Port: `9000`
- Storage directory: `./storage`

#### Command Line Options

| Option | Short | Description | Default |
|--------|-------|-------------|---------|
| `--config` | `-c` | Load configuration from JSON file | (none) |
| `--address` | `-a` | IP address to bind to | `0.0.0.0` |
| `--port` | `-p` | Server port | `9000` |
| `--storage` | `-s` | Directory for file storage | `./storage` |
| `--keys` | `-k` | Access keys file (CSV format) | `./access_keys.csv` |
| `--users` | `-u` | Users file (CSV format) | `./users.csv` |
| `--no-auth` | | Disable authentication (allows anonymous access) | `false` |
| `--ssl` | `-S` | Enable SSL/TLS (HTTPS) with self-signed certificate | `false` |
| `--letsencrypt`, `-L` | | Use Let's Encrypt certificates from directory | (none) |
| `--cert` | | SSL certificate file (alternative to --letsencrypt) | `./certs/server.crt` |
| `--key` | | SSL private key file (alternative to --letsencrypt) | `./certs/server.key` |
| `--log-level` | `-l` | Log verbosity level (0-4) | `0` |
| `--log-dir` | | Directory for log files | (current directory) |
| `--no-cors` | | Disable CORS (default: enabled with permissive settings) | `false` |
| `--cors-origins` | | Comma-separated allowed origins | `*` |
| `--cors-methods` | | Comma-separated allowed HTTP methods | `GET,POST,PUT,DELETE,OPTIONS,HEAD` |
| `--cors-headers` | | Comma-separated allowed headers | `Content-Type,Authorization,X-Amz-Date,X-Amz-Security-Token,X-Requested-With,X-Access-Key` |
| `--cors-exposed-headers` | | Comma-separated exposed headers | `ETag,X-File-Size,X-Upload-Id` |
| `--cors-credentials` | | Allow credentials (true/false) | `false` |
| `--cors-max-age` | | Preflight cache duration in seconds | `86400` |
| `--help` | `-h` | Show help message | - |

#### Configuration File

For complex deployments, you can use a JSON configuration file to set all server options. The configuration file supports all command-line options and additional fine‑grained settings (rate limiting, keep‑alive, upload limits, CORS, etc.).

**Usage:**

```bash
./s3_server --config /path/to/config.json
```

Command‑line arguments override values from the configuration file.

**File format:** See [`config.example.json`](config.example.json) for a complete example with all available fields and their default values.

The configuration file includes the following sections:

- **Server basics** (`address`, `port`, `storage_path`, `keys_file`, `users_file`, `enable_auth`, `enable_ssl`, `use_letsencrypt`)
- **SSL configuration** (`ssl`) – certificate paths and client verification
- **CORS configuration** (`cors`) – allowed origins, methods, headers, etc.
- **Upload limits** (`upload_limits`) – maximum file size, part size, etc.
- **Keep‑alive settings** (`keep_alive`) – connection reuse parameters
- **Rate limiting** (`rate_limiter`) – request/connection limits and DDoS protection
- **Logging** (`logging`) – log level and directory

All fields are optional; missing fields use the built‑in defaults.

#### Example Usage

1. **Run on specific port with custom storage:**

   ```bash
   ./s3_server --port 8080 --storage /mnt/data/storage
   ```

2. **Run with HTTPS (SSL/TLS) - Self-signed certificate:**

   ```bash
   ./s3_server --ssl --cert ./certs/server.crt --key ./certs/server.key
   ```

   *Note: The server will automatically generate and renew self-signed certificates if they don't exist or are expiring soon.*

3. **Run with HTTPS (SSL/TLS) - Let's Encrypt certificate:**

   ```bash
   # Using default Let's Encrypt directory (/etc/letsencrypt/live/<domain>)
   ./s3_server --letsencrypt /etc/letsencrypt/live/example.com
   
   # Or with short option
   ./s3_server -L /etc/letsencrypt/live/example.com
   ```

   *Note: Let's Encrypt certificates must be obtained separately using certbot. The server will check certificate expiration and warn if renewal is needed.*

4. **Run without authentication (development mode):**

   ```bash
   ./s3_server --no-auth
   ```

5. **Configure CORS for specific origins:**

   ```bash
   # Allow only specific origins with custom headers
   ./s3_server --cors-origins "https://example.com,http://localhost:3000" \
               --cors-headers "Content-Type,Authorization,X-Access-Key" \
               --cors-credentials true \
               --cors-max-age 3600
   ```

   *Note: CORS is enabled by default with permissive settings (`*` origin). Use `--no-cors` to disable CORS entirely.*

6. **Run with verbose logging:**

   ```bash
   ./s3_server --log-level 2 --log-dir ./logs
   ```

7. **Run in background:**

   ```bash
   nohup ./s3_server > server.log 2>&1 &
   ```

### Accessing the Application

1. **Web Interface:** Open your browser and navigate to `http://localhost:9000` (or `https://localhost:9000` if SSL is enabled)
2. **API Endpoint:** The S3-compatible API is available at `http://localhost:9000` (or `https://localhost:9000` with SSL)

### REST API Reference

The server provides a RESTful API with S3-like semantics. For complete API documentation, see the OpenAPI specification at `/openapi.yaml` or `/api/spec`.

#### Core File Operations

##### Upload a File (PUT)

```bash
curl -X PUT http://localhost:9000/my-file.txt \
  -H "Content-Type: application/octet-stream" \
  --data-binary @local-file.txt
```

##### Download a File (GET)

```bash
curl -X GET http://localhost:9000/my-file.txt -o downloaded-file.txt
```

##### List Files (GET /list)

```bash
curl -X GET http://localhost:9000/list
```

##### Delete a File (DELETE)

```bash
curl -X DELETE http://localhost:9000/my-file.txt
```

#### Multipart Upload Operations

For large files (> 100MB), use multipart upload:

##### 1. Initiate Multipart Upload

```bash
curl -X POST http://localhost:9000/upload/initiate \
  -H "Content-Type: application/json" \
  -d '{"filename": "large-file.zip", "part_size": 5242880}'
```

##### 2. Upload Part

```bash
curl -X PUT "http://localhost:9000/upload/part?upload_id=UUID&part_number=1" \
  -H "Content-Type: application/octet-stream" \
  --data-binary @part1.bin
```

##### 3. Complete Upload

```bash
curl -X POST http://localhost:9000/upload/complete \
  -H "Content-Type: application/json" \
  -d '{"upload_id": "UUID", "parts": [{"part_number": 1, "etag": "abc123"}]}'
```

##### 4. Check Upload Progress

```bash
curl -X GET "http://localhost:9000/upload/progress?upload_id=UUID"
```

##### 5. Abort Upload

```bash
curl -X DELETE "http://localhost:9000/upload/abort?upload_id=UUID"
```

#### Authentication

When authentication is enabled, include the `Authorization` header with AWS Signature Version 4:

```bash
curl -X GET http://localhost:9000/list \
  -H "Authorization: AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20250101/us-east-1/s3/aws4_request, SignedHeaders=host;x-amz-date, Signature=calculated-signature"
```

#### OpenAPI Specification

The complete API documentation is available in OpenAPI 3.0 format:

- `GET /openapi.yaml` - YAML format
- `GET /api/spec` - JSON format

### Using with S3 Clients

The server provides basic S3 API compatibility. Example using AWS CLI:

```bash
# Configure AWS CLI to use custom endpoint
aws configure set default.s3.endpoint_url http://localhost:9000

# List files (uses /list endpoint internally)
aws s3 ls s3:///

# Upload a file
aws s3 cp local-file.txt s3://my-file.txt
```

**Note:** The server implements a simplified S3 API without bucket support. All files are stored in a flat namespace.

## 5. Road Map of Product Development

### Phase 1: Core Stability & Security (Current)

- [x] Basic S3 API implementation (PUT, GET, DELETE, LIST)
- [x] Multipart upload support
- [x] Stream upload support (incremental file writing)
- [x] Web interface for file management
- [x] Path traversal protection
- [x] AWS Signature Version 4 authentication
- [x] Role-based access control (RBAC) with user management
- [x] HTTPS/SSL support with automatic certificate generation
- [x] Rate limiting and DDoS protection

### Phase 2: Enhanced Features & Performance

- [ ] Bucket management (create, delete, configure buckets)
- [ ] Object versioning and lifecycle policies
- [ ] Server-side encryption
- [x] CORS configuration
- [ ] Metadata support for objects
- [ ] Compression (gzip/brotli) for transfers
- [ ] Cache layer for frequently accessed files
- [x] Connection pooling and keep-alive optimization

### Phase 3: Monitoring & Administration

- [ ] Comprehensive logging with log rotation
- [ ] Prometheus metrics endpoint
- [ ] Health check endpoints
- [ ] Admin API for system management
- [ ] Backup and restore functionality
- [ ] User management interface
- [ ] Audit logging for security compliance

### Phase 4: Scalability & Advanced Features

- [ ] Database backend for metadata (SQLite/PostgreSQL)
- [ ] Replication between multiple instances
- [ ] CDN integration support
- [ ] Docker containerization and Docker Compose setup
- [ ] Kubernetes deployment manifests
- [ ] Client SDKs for Python, JavaScript, Go
- [ ] WebSocket support for real-time updates

### Phase 5: Enterprise & Ecosystem

- [ ] LDAP/Active Directory integration
- [ ] OAuth2/OIDC authentication
- [ ] Webhook notifications for file events
- [ ] Plugin system for custom storage backends
- [ ] S3 Select support (SQL queries on objects)
- [ ] Glacier-like cold storage tier
- [ ] Geographic replication

### Current Known Issues & Limitations

- **Authentication**: AWS Signature Version 4 is implemented but requires proper key management
- **HTTPS/SSL**: SSL/TLS support with automatic self-signed certificate generation and Let's Encrypt integration
- **Error Handling**: Basic error handling is implemented; some edge cases may need improvement
- **File Size Validation**: No built-in file size limits or quota management
- **Temporary Files**: Multipart and stream uploads create temporary files that are cleaned up on completion/abort, but orphaned files may accumulate on unexpected crashes
- **Memory Usage**: Large file uploads buffer data in memory; streaming improvements needed for very large files
- [X] **Concurrency Limits**: Rate limiting and connection throttling are implemented with periodic cleanup of stale IP entries (every 5 minutes). Configuration is static per server start.
- **User Management**: Basic RBAC is implemented but lacks advanced features like groups, permission inheritance, and fine-grained resource controls

### Contributing

We welcome contributions! Please see our [Contributing Guidelines](CONTRIBUTING.md) for details.

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

### Support

- **Documentation**: [Project Wiki](https://github.com/Moneypoolator/home-simstoser/wiki)
- **Issues**: [GitHub Issues](https://github.com/Moneypoolator/home-simstoser/issues)
- **Discussions**: [GitHub Discussions](https://github.com/Moneypoolator/home-simstoser/discussions)

### Acknowledgments

- **Boost** for networking and asynchronous I/O
- **nlohmann/json** for JSON parsing and serialization
- **OpenSSL** for cryptographic operations
- **AWS S3** for API design inspiration
- **React & Vite** for the modern web interface
- **Tailwind CSS** for styling utilities
