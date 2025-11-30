# Cachify - Redis-like In-Memory Key-Value Store

Cachify is a lightweight, multithreaded, Redis-inspired in-memory key-value store written in C++. It supports TTL-based key expiration, concurrent client access via a custom TCP server, and a simple text-based command protocol. Cachify is ideal for learning in-memory databases, caching layers, or building fast key-value storage solutions.

---

## Project Structure

```
Cachify/
├── Makefile            # Build instructions
├── README.md           # Project overview
├── cachify_client      # TCP client executable
├── cachify_server      # TCP server executable
├── commands.txt        # List of supported commands
├── include/            # Header files
│   └── kv_store.h      # KVStore class declaration
└── src/                # Source code files
    ├── client.cpp      # Client implementation
    ├── server.cpp      # Server implementation
    └── kv_store.cpp    # KVStore class definition
```

---

## Features

* **In-memory key-value storage** with thread-safe access
* **TTL-based key expiration** with a background cleaner thread
* **Multithreaded TCP server** allowing multiple clients to connect concurrently
* **Simple text-based protocol**:

  * `SET key value [ttl_seconds]`
  * `GET key`
  * `DEL key`
  * `SIZE`
  * `PING`
  * `QUIT`
* **Flexible stripes** for concurrent access (default: 128 stripes)

---

## Build Instructions

### 1. Clone the Repository

```bash
git clone <repo-url>
cd cachify
```

### 2. Build the Project

```bash
make
```

This will generate two executables:

* `cachify_server` → The server
* `cachify_client` → The client

---

## Running the Server

Start the server on a specific port (default: `6379`):

```bash
./cachify_server 6379
```

* If no port is provided, it defaults to `6379`.
* The server supports multiple concurrent clients using threads.

---

## Connecting with the Client

Connect to the server using the client executable:

```bash
./cachify_client 127.0.0.1 6379
```

* Default host is `127.0.0.1` and default port is `6379` if not provided.
* You can now type commands interactively.

---

## Supported Commands

* `SET key value [ttl_seconds]` – Set a key with an optional TTL (time-to-live in seconds)
* `GET key` – Retrieve the value of a key
* `DEL key` – Delete a key
* `SIZE` – Return the total number of keys
* `PING` – Check server connectivity (returns `PONG`)
* `QUIT` – Disconnect the client

---

## Example Session

```
> SET myKey "Hello World" 60
+OK
> GET myKey
$11
Hello World
> SIZE
:1
> DEL myKey
+OK
> GET myKey
$-1
> QUIT
```

---

## How Cachify Works Internally

* **KVStore class**:

  * Uses striped hash maps and mutexes for thread-safe storage.
  * Each key can optionally have a TTL (expiry time).
  * Expired keys are cleaned by a dedicated background cleaner thread.
* **Server**:

  * Listens on a TCP port and spawns a thread per client.
  * Processes simple text commands and sends structured responses.
* **Client**:

  * Connects to server and sends/receives text-based commands interactively.

---

## License

This project is licensed under the MIT License.
