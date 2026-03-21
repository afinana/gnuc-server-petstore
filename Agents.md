# Petstore API Agent Context

## 1. Project Overview
This project is a C RESTful API implementing the [Swagger Petstore v2](https://petstore.swagger.io/v2/swagger.json) using:
- **`libmicrohttpd`** — HTTP server with thread pool
- **`libmongoc` / `libbson`** — MongoDB C Driver for all data operations

Endpoints served under `/v2/pet` and `/v2/user` follow the Swagger spec.

## 2. Architecture

```
main.c  →  request_handler (routing)
               ↓
         handlers.c  (JSON↔BSON, business logic)
               ↓
         database.c  (mongoc client pool CRUD)
```

- **Routing** (`main.c`): URL/method matching → calls handler functions. Upload data is accumulated for POST/PUT.
- **Handlers** (`handlers.c`): Parse JSON to BSON via `bson_new_from_json`, build MongoDB filters, call `db_*` functions, serialize results back to JSON.
- **Database** (`database.c`): Thread-safe `mongoc_client_pool_t`; exposes `db_insert_one`, `db_find_one`, `db_find`, `db_update_one`, `db_delete_one`.

## 3. Key Files
| File | Purpose |
|------|---------|
| `main.c` | Entry point, HTTP daemon setup, request routing |
| `handlers.h/c` | Business logic: pet CRUD, user CRUD, login/logout |
| `database.h/c` | MongoDB abstraction (client pool, CRUD operations) |
| `log-utils.h` | Timestamped logging macros |
| `Makefile` | Build with `pkg-config --cflags/--libs libmongoc-1.0` |
| `Dockerfile` | Multi-stage build → `debian:bookworm-slim` runtime |
| `config.json` | Default MongoDB connection config |

## 4. Environment Variables
| Variable | Default | Description |
|----------|---------|-------------|
| `MONGO_URI` | `mongodb://localhost:27017` | MongoDB connection string |
| `PORT` | `8080` | HTTP listen port |

## 5. Agent Instructions
1. **Build**: `make clean && make` — must compile with zero errors and warnings
2. **Swagger compliance**: All routes must match [petstore swagger spec](https://petstore.swagger.io/v2/swagger.json)
3. **MongoDB only**: No Redis, no cJSON — use `libmongoc`/`libbson` exclusively
4. **Memory safety**: Every `bson_t*` from `bson_copy`/`BCON_NEW`/`bson_new_from_json` must be destroyed. Every `strdup`/`malloc` must be `free`d
5. **Separation of concerns**: Routing in `main.c`, business logic in `handlers.c`, raw DB calls in `database.c`

## 6. Build Dependencies
```bash
# Debian/Ubuntu
sudo apt-get install libmicrohttpd-dev libmongoc-dev

# Gentoo
sudo emerge net-libs/libmicrohttpd dev-libs/mongo-c-driver
```
