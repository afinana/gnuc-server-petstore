# Petstore API Agent Context

## 1. Project Overview
This project is a glib C RESTful API implementing the [Swagger Petstore API v2](https://petstore.swagger.io/v2/swagger.json) using `libmicrohttpd` as the underlying HTTP server and **MongoDB** as the central database.

The application serves the standard `/v2/pet` and `/v2/user` endpoints defined by the Swagger specification to perform CRUD operations on `pets` and `users`.

## 2. Architecture & Code Health
**CRITICAL CONTEXT**: The repository's data layer uses the MongoDB C Driver (`libmongoc`) and BSON library (`libbson`) to handle operations.
Be aware that the codebase currently suffers from compilation errors. There are lingering redundant logic paths and includes leftover from a previous Redis implementation (such as `hiredis` mentions in `database.c` and `cJSON` functions in `handlers.c`). The top priority for any AI agent interacting with this repo is to recognize that MongoDB is the correct active backend, and any Redis/cJSON specific anomalies should be cleaned up.

## 3. Technology Stack
- **Web Server Core**: `libmicrohttpd`
- **Data Store**: MongoDB
- **MongoDB Driver**: `libmongoc` / BSON (`libbson`)

## 4. Key Files
- `main.c`: The entry point. Sets up the HTTP Daemon (`libmicrohttpd`), initializes the MongoDB configuration with the connection string, and routes HTTP method parsing to `handlers.c`.
- `handlers.c & handlers.h`: Controller logic mapping HTTP requests/payloads to specific CRUD endpoints.
- `database.c & database.h`: Database abstraction layer handling direct interaction with MongoDB collections (`mongoc_collection_*`) and processing BSON results. Needs cleanup.
- `log-utils.h`: Logging utilities.
- `Makefile`: Build config file linking the application against `-lmongoc-1.0 -lbson-1.0 -lmicrohttpd`.

## 5. Agent Instructions / Standard Operating Procedures
When making modifications in this repository, strictly abide by the following instructions:
1. **Always Verify Compilation Status**: Run `make clean && make` to check for compilation errors frequently.
2. **Adhere closely to the active Swagger Spec**: Ensure all routing algorithms and JSON payloads (both inbound and outbound) strictly map to the [Swagger Petstore specifications](https://petstore.swagger.io/v2/swagger.json).
3. **Assert MongoDB as the sole source of truth**: When interacting with the data access layer, utilize MongoDB and BSON functions (`bson_t`, `mongoc_client_t`). Actively discard functions and logic explicitly relating to `redisContext` or `cJSON` when fixing compilation errors.
4. **Follow the C Design Pattern**: Routing in `main.c` forwards variables to logical endpoints in `handlers.c` decorators (e.g., `handle_post_pet`), which assemble documents and interact with `db_*` CRUD routines in `database.c`. Maintain this separation.

## 6. Development Dependencies
*Ensure these packages are installed for successful compilation:*
- `libmicrohttpd-dev`
- `libmongoc-1.0-dev`
- `libbson-1.0-dev`
