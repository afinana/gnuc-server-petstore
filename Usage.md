Hre is a quick guide on how to run and use the **Petstore API** locally.

### 1. Requirements

You'll need the `mongoc` and [bson](cci:1://file:///home/afinana/development/github/gnuc-server-petstore/handlers.c:26:0-38:1) drivers installed, as well as `libmicrohttpd`.
You must have a MongoDB instance running. If you're using Docker/Podman, you can start one on the host network easily:
```bash
podman run -d --name mongo-petstore --network=host docker.io/library/mongo:7
```

### 2. Build and Start the Server

```bash
# 1. Compile the server
make clean && make

# 2. Run the server, connecting it to your local MongoDB
MONGO_URI="mongodb://127.0.0.1:27017/?directConnection=true" ./petstore-api
```
*(The server will start listening on port 8080 by default. You can change this by exporting `PORT=9000`)*.

### 3. Usage Examples (interacting via `curl`)

#### **Creating a Pet**
```bash
curl -X POST http://localhost:8080/v2/pet \
  -H "Content-Type: application/json" \
  -d '{
    "id": 101,
    "name": "doggie",
    "status": "available",
    "tags": [{"id": 1, "name": "friendly"}]
  }'
```

#### **Getting a Pet by ID**
```bash
curl http://localhost:8080/v2/pet/101
```

#### **Finding Pets by Status / Tags**
*(Note: Supports comma-separated lists for multiple values)*
```bash
# Find all available pets
curl "http://localhost:8080/v2/pet/findByStatus?status=available"

# Find pets that are both available or pending
curl "http://localhost:8080/v2/pet/findByStatus?status=available,pending"

# Find pets by a specific tag
curl "http://localhost:8080/v2/pet/findByTags?tags=friendly"
```

#### **Creating / Retrieving a User**
```bash
# Create user
curl -X POST http://localhost:8080/v2/user \
  -H "Content-Type: application/json" \
  -d '{
    "id": 1,
    "username": "john_doe",
    "firstName": "John",
    "email": "john@example.com",
    "password": "secretpassword"
  }'

# Get user
curl http://localhost:8080/v2/user/john_doe
```

#### **User Login/Logout**
```bash
# Login (returns a success message if password matches)
curl "http://localhost:8080/v2/user/login?username=john_doe&password=secretpassword"

# Logout
curl "http://localhost:8080/v2/user/logout"
```

### 4. Running the Benchmark

There is also a bundled load testing script ([benchmark.sh](cci:7://file:///home/afinana/development/github/gnuc-server-petstore/benchmark.sh:0:0-0:0)) which will seed test data and spam the endpoints to test performance. You can use it like this:

```bash
# Usage: ./benchmark.sh [TOTAL_REQUESTS_PER_ENDPOINT] [CONCURRENCY]
./benchmark.sh 200 10
```