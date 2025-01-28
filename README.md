### Introduction

This project implements a RESTful pet store API using C, the MongoDB C driver, and a Makefile for build automation. The API will include the following endpoints:

1. **Routes**:
   - **POST `/pet`**: Creates a new pet using `create_pet`.
   - **PUT `/pet`**: Updates an existing pet using `update_pet`.
   - **DELETE `/pet/{id}`**: Deletes a pet by its ID using `delete_pet`.
   - **GET `/pet/findByTags`**: Retrieves pets by tags using `find_by_tags`.
   - **GET `/pet/findByState`**: Retrieves pets by state using `find_by_state`.

2. **Microhttpd**:
   - The `MHD_Daemon` starts a server that listens on the specified port.
   - Incoming requests are routed based on their `method` and `url`.

3. **Database Initialization**:
   - `db_init` initializes the MongoDB connection.
   - `db_cleanup` closes the connection.

4. **Memory Management**:
   - JSON payloads are processed dynamically, ensuring efficient use of memory.


### Install Dependencies**
Ensure you have `libmicrohttpd` and `libmongoc-dev` installed. Use a package manager (e.g., `apt`, `yum`, or `brew`) to install it:
```bash
sudo apt-get install libmicrohttpd-dev libmongoc-dev
```


### Compilation

From Unix terminal using gcc:
```bash
gcc main.c database.c handlers.c -o server -lmicrohttpd -lbson-1.0 -lmongoc-1.0 -o petstore-api
```


From Unix terminal using make:
```bash
make all
```

---

### Running the Server

Run the compiled server:
```bash
./server
```

The server will listen on `http://localhost:8888`. You can test it with tools like `curl` or Postman:

- **POST**:
  ```bash
  curl -X POST -d '{"id":2,"name":"cat2"}' http://localhost:8888/pet
  ```
- **PUT**:
  ```bash
  curl -X PUT -d '{"id":2,"name":"updatedCat"}' http://localhost:8888/pet
  ```
- **DELETE**:
  ```bash
  curl -X DELETE http://localhost:8888/pet/2
  ```
- **GET by tags**:
  ```bash
  curl "http://localhost:8888/pet/findByTags?tags=tag02"
  ```
- **GET by state**:
  ```bash
  curl "http://localhost:8888/pet/findByState?state=available"
  ```

  
---

### **Dockerfile with Build Process**

We create a **multi-stage Dockerfile** that includes the build process in the container itself. This approach uses a builder stage to compile the application and then copies the statically compiled binary into a minimal `scratch` image.


```dockerfile
# Stage 1: Builder
FROM gcc:latest AS builder

# Set the working directory
WORKDIR /app

# Copy the source code into the container
COPY . /app

# Install necessary libraries for building
RUN apt-get update && apt-get install -y \
    libmicrohttpd-dev \
    libbson-dev \
    libmongoc-dev \
    && rm -rf /var/lib/apt/lists/*

# Build the application as a static binary
RUN gcc -static main.c database.c handlers.c -o server -lmicrohttpd -lbson-1.0 -lmongoc-1.0 -o petstore-api

# Stage 2: Final image
FROM scratch

# Set the working directory
WORKDIR /

# Copy the statically compiled binary from the builder stage
COPY --from=builder /app/server /

# Copy necessary certificates for MongoDB (if needed)
COPY --from=builder /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/ca-certificates.crt

# Expose the application port
EXPOSE 8888

# Command to run the application
CMD ["/server"]
```

---

### **Explanation**

1. **Builder Stage**:
   - The `gcc:latest` image is used as the build environment.
   - The source code is copied into the container.
   - Required development libraries (`libmicrohttpd-dev`, `libbson-dev`, `libmongoc-dev`) are installed.
   - The application is compiled into a statically linked binary using `-static`.

2. **Final Stage**:
   - The `scratch` image is used to create a minimal container.
   - The statically compiled binary (`server`) is copied from the builder stage.
   - Optional: If your application connects to MongoDB over TLS, the CA certificates are also copied.

3. **Exposed Port**:
   - The application listens on port `8888`.

4. **Run Command**:
   - `CMD ["/server"]` starts the application.

---

### **Build and Run the Docker Image**

```bash
# Build the Docker image
docker build -t petstore-api .

# Run the Docker container
docker run -p 8888:8888 petstore-api
```

---

### **Testing**

After running the container, test the API as usual:

```bash
curl -X POST -d '{"id":2,"name":"cat2"}' http://localhost:8888/pet
```

---
## **Mongodb queries**

- Get all pets:
```bash
db.getCollection("pets").find({})
```

- Find pets of name='cat1'
```bash
db.getCollection("pets").find({'name':'cat1'})
```
- Find pets of category.name='category01'
```bash
db.getCollection("pets").find({'category.name':'category01'})
```

- Find pets with tags : 'tag01' or 'tag02'
```bash
db.getCollection("pets").find({
    tags: {
        $elemMatch: { name: "tag01" },
    },
    tags: {
        $elemMatch: { name: "tag02" },
    }
})
```

- Find pets with status 'available' or 'sold'
```bash
db.getCollection("pets").find({
    status: { $in: ["available", "sold"] }
});
```
---

### **References**

- [MongoDB C Driver](http://mongoc.org/libmongoc/current/index.html)
- BSON Library: [libbson](http://mongoc.org/libbson/current/index.html)
- BSON Examples: https://mongoc.org/libbson/1.21.0/include-and-link.html
- Microhttpd: [GNU libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/)