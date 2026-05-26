# Petstore API — GNU C Server

A **RESTful Pet Store API** built in **GNU C**, using **Redis** for data persistence, **libmicrohttpd** for HTTP serving, and **cJSON/libbson** for JSON processing.

Provides full CRUD endpoints for managing pets and users, with a thread-pooled HTTP server, a Redis connection pool, and secondary indexes for efficient tag/status lookups.

---

## API Endpoints

### System

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/health` | Health check — returns `{"status":"ok"}` |

### Pet Routes (`/v2/pet`)

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/v2/pet` | Create a new pet |
| `PUT` | `/v2/pet` | Update an existing pet |
| `GET` | `/v2/pet` | List all pets |
| `DELETE` | `/v2/pet/{id}` | Delete a pet by ID |
| `GET` | `/v2/pet/{id}` | Get a pet by ID |
| `GET` | `/v2/pet/findByTags?tags=tag1,tag2` | Find pets by tags |
| `GET` | `/v2/pet/findByStatus?status=available,sold` | Find pets by status |

### User Routes (`/v2/user`)

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/v2/user` | Create a new user |
| `PUT` | `/v2/user` | Update an existing user |
| `GET` | `/v2/user` | List all users |
| `DELETE` | `/v2/user/{id}` | Delete a user by ID or username |
| `GET` | `/v2/user/{id}` | Get a user by numeric ID |
| `GET` | `/v2/user/findByName/{username}` | Get a user by username |
| `POST` | `/v2/user/login` | Login — validates username/password against DB |
| `POST` | `/v2/user/logout?username=alice` | Logout |

---

## Architecture

```
main.c             → HTTP server, routing, Content-Type validation, body limit
handlers.c         → Business logic (pet/user CRUD), JSON↔BSON conversion
handlers.h         → Public handler function declarations
database.c         → Redis CRUD operations via thread-safe connection pool
database.h         → Database function declarations
database_utils.c   → URI parsing + ID extraction (testable without Redis)
database_utils.h   → database_utils declarations
log-utils.h        → Thread-safe logging macros with millisecond timestamps
Makefile            → Build configuration (app + two test binaries)
Dockerfile          → Multi-stage Docker build
```

---

## Dependencies

| Library | Purpose |
|---------|---------|
| [libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/) | HTTP server with thread pool |
| [hiredis](https://github.com/redis/hiredis) | Redis client |
| [libbson](https://github.com/mongodb/libbson) | BSON document model (query/result representation) |
| [cJSON](https://github.com/DaveGamble/cJSON) | Lightweight JSON parsing |

### Install Dependencies (Debian/Ubuntu)

```bash
sudo apt-get install libmicrohttpd-dev libhiredis-dev libbson-dev libcjson-dev
```

---

## Build & Run

### Compile

```bash
make all        # Release build
make debug      # Debug build (no optimization, with DEBUG macro)
make clean      # Remove build artifacts
```

### Run

```bash
# Default: listens on port 8080, connects to localhost Redis
./petstore-api

# Custom configuration via environment variables
port=9090 REDIS_URL="redis://:mypassword@redis-host:6379/" ./petstore-api
```

The server will listen on `http://localhost:8080` by default.

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `port` | `8080` | HTTP listen port |
| `REDIS_URL` | `redis://:middlelandPassword01@127.0.0.1:6379/` | Redis connection URI |

---

## Testing

The project has two independent test suites, both running without a live Redis instance.

```bash
make test           # Handler unit tests   (tests/test_runner)
make test-utils     # DB utils unit tests  (tests/test_runner_utils)
make test-all       # Run both suites
make valgrind       # Both suites under Valgrind
```

### Test architecture

| Binary | Coverage | Dependencies |
|--------|----------|--------------|
| `tests/test_runner` | All handler functions via Redis stubs | hiredis, libbson, libmicrohttpd, cjson |
| `tests/test_runner_utils` | `parse_redis_uri`, `extract_id_string` | libbson only |

---

## Docker

### Build & Run

```bash
# Build the Docker image
docker build -t petstore-api .

# Run with a Redis instance
docker run -p 8080:8080 \
  -e REDIS_URL="redis://:password@redis-host:6379/" \
  petstore-api
```

### Test

```bash
# Health check
curl http://localhost:8080/health

# Create a pet
curl -X POST -H "Content-Type: application/json" \
  -d '{"id":1,"name":"Buddy","status":"available","tags":[{"name":"dog"}]}' \
  http://localhost:8080/v2/pet

# Get pet by ID
curl http://localhost:8080/v2/pet/1

# Find by status
curl "http://localhost:8080/v2/pet/findByStatus?status=available"

# Create a user (login password stored as plaintext — hash before production use)
curl -X POST -H "Content-Type: application/json" \
  -d '{"id":1,"username":"alice","password":"s3cr3t","email":"alice@example.com"}' \
  http://localhost:8080/v2/user

# Login
curl -X POST -H "Content-Type: application/json" \
  -d '{"username":"alice","password":"s3cr3t"}' \
  http://localhost:8080/v2/user/login
```

---

## References

- [GNU libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/)
- [hiredis](https://github.com/redis/hiredis)
- [libbson](https://github.com/mongodb/libbson)
- [cJSON](https://github.com/DaveGamble/cJSON)
