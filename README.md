# Petstore API — GNU C Server

A **RESTful Pet Store API** built in **GNU C**, using **MongoDB** for database operations, **libmicrohttpd** for HTTP serving, and **cJSON/libbson** for JSON processing.

Provides full CRUD endpoints for managing pets and users, with a thread-pooled HTTP server and MongoDB connection pooling for concurrent request handling.

---

## API Endpoints

### Pet Routes (`/v2/pet`)

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/v2/pet` | Create a new pet |
| `PUT` | `/v2/pet` | Update an existing pet |
| `DELETE` | `/v2/pet/{id}` | Delete a pet by ID |
| `GET` | `/v2/pet/{id}` | Get a pet by ID |
| `GET` | `/v2/pet/findByTags?tags=tag1,tag2` | Find pets by tags |
| `GET` | `/v2/pet/findByStatus?status=available,sold` | Find pets by status |

### User Routes (`/v2/user`)

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/v2/user` | Create a new user |
| `PUT` | `/v2/user` | Update an existing user |
| `DELETE` | `/v2/user/{username}` | Delete a user by username |
| `GET` | `/v2/user` | List all users |
| `GET` | `/v2/user/{username}` | Get a user by username |
| `POST` | `/v2/user/login` | User login |
| `POST` | `/v2/user/logout` | User logout |

---

## Architecture

```
main.c         → HTTP server, request routing, signal handling
handlers.c     → Business logic (pet/user CRUD), JSON↔BSON conversion
handlers.h     → Public handler function declarations
database.c     → MongoDB CRUD operations via connection pool
database.h     → Database function declarations
log-utils.h    → Thread-safe logging macros with millisecond timestamps
Makefile        → Build configuration
Dockerfile      → Multi-stage Docker build
```

---

## Dependencies

| Library | Purpose |
|---------|---------|
| [libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/) | HTTP server with thread pool |
| [MongoDB C Driver](https://mongoc.org/) (mongoc + libbson) | MongoDB client with connection pooling |
| [cJSON](https://github.com/DaveGamble/cJSON) | JSON parsing (login handler) |

### Install Dependencies (Debian/Ubuntu)

```bash
sudo apt-get install libmicrohttpd-dev libmongoc-dev libcjson-dev
```

---

## Build & Run

### Compile

```bash
make all        # Release build
make debug      # Debug build (no optimization, debug symbols)
make clean      # Remove build artifacts
```

### Run

```bash
# Default: listens on port 8080, connects to localhost MongoDB
./petstore-api

# Custom configuration via environment variables
port=9090 mongoURI="mongodb://user:pass@dbhost:27017/admin" ./petstore-api
```

The server will listen on `http://localhost:8080` by default.

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `port` | `8080` | HTTP listen port |
| `mongoURI` | `mongodb://root@127.0.0.1:27017/admin?...` | MongoDB connection string |

---

## Docker

### Build & Run

```bash
# Build the Docker image
docker build -t petstore-api .

# Run with MongoDB connection
docker run -p 8080:8080 \
  -e mongoURI="mongodb://user:pass@host:27017/admin" \
  petstore-api
```

### Test

```bash
# Create a pet
curl -X POST -H "Content-Type: application/json" \
  -d '{"id":1,"name":"Buddy","status":"available","tags":[{"name":"dog"}]}' \
  http://localhost:8080/v2/pet

# Get pet by ID
curl http://localhost:8080/v2/pet/1

# Find by status
curl "http://localhost:8080/v2/pet/findByStatus?status=available"
```

---

## References

- [GNU libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/)
- [MongoDB C Driver](https://mongoc.org/)
- [cJSON](https://github.com/DaveGamble/cJSON)
