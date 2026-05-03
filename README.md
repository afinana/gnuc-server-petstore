### Introduction

This project is a **RESTful Pet Store API** built in **glib C**, using **Redis** for database operations and **cJSON** for JSON processing.

It provides endpoints to manage pet data, including creation, retrieval, updating, and deletion. 

The project is structured with a **Makefile** for efficient build automation, ensuring smooth compilation and dependency management.

The API includes the following endpoints:

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
   - `db_init` initializes the Redis connection.
   - `db_cleanup` closes the connection.

4. **Memory Management**:
   - JSON payloads are processed dynamically using cJSON, ensuring efficient use of memory.

### Install Dependencies

Ensure you have `libmicrohttpd`, `hiredis`, and `cjson` installed. Use a package manager (e.g., `apt`, `yum`, or `brew`) to install them:


```bash
sudo apt-get install libmicrohttpd-dev libhiredis-dev libcjson-dev
```


### Compilation

From Unix terminal using gcc:
```bash
gcc main.c database.c handlers.c -o server -lmicrohttpd -lhiredis -lcjson -o petstore-api
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

The server will listen on `http://localhost:8080`. You can test it with tools like `curl` or Postman:


---

### **Dockerfile with Build Process**

We create a **multi-stage Dockerfile** that includes the build process in the container itself. This approach uses a builder stage to compile the application and then copies the statically compiled binary into a minimal `scratch` image.


1. **Builder Stage**:
   - The `gcc:latest` image is used as the build environment.
   - The source code is copied into the container.
   - Required development libraries (`libmicrohttpd-dev`, `libbson-dev`, `libmongoc-dev`) are installed.


2. **Final Stage**:
   - The `debian:buster-slim` image is used to create a minimal container.
   - The statically compiled binary (`server`) is copied from the builder stage.
   - Optional: If the application connects to MongoDB over TLS, the CA certificates are also copied.

3. **Exposed Port**:
   - The application listens on port `8080`.

4. **Run Command**:
   - `CMD ["/app/petstore-server"]` starts the application.

---

### **Build and Run the Docker Image**

```bash
# Build the Docker image
docker build . -t petstore-api 

# Run the Docker container
docker run -p 8080:8080 -e redisURI="redis://password@localhost:27017" petstore-api```

---

### **Testing**

After running the container, test the API as usual:

```bash
curl -X POST -d '{"id":2,"name":"cat2"}' http://localhost:8888/pet
```

---

### **References**

- Microhttpd: [GNU libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/)
- Hiredis: [Hiredis GitHub](https://github.com/redis/hiredis)
- cJSON: [cJSON GitHub](https://github.com/DaveGamble/cJSON)
