# Taiwanese Enterprises LLC backend

A C++ web server for the Taiwanese Enterprises LLC website, built with the
[Crow](https://github.com/CrowCpp/Crow) single header framework. It serves the
static site and exposes a small JSON API for quote requests. Written at a
COP 3330 level on purpose: plain classes, separate headers and sources,
comments that explain the why.

## Layout

```
backend/
  third_party/crow.h      the Crow framework, single header
  src/models.hpp          Service and QuoteRequest classes
  src/quote_store.hpp     SQLite storage interface
  src/quote_store.cpp     SQLite storage implementation
  src/main.cpp            routes, static file serving, server startup
  CMakeLists.txt          C++17 build, links sqlite3 and pthread
  te-backend.service      systemd unit for EC2
  scripts/ec2-user-data.sh one shot EC2 setup script
```

## Build locally

You need g++, CMake, and the SQLite dev headers.

Ubuntu:

```
sudo apt-get install -y build-essential cmake libsqlite3-dev
```

Then from the repo root:

```
cmake -S backend -B backend/build
cmake --build backend/build
```

That produces `backend/build/te_server`.

## Run locally

```
cd backend/build
./te_server
```

The server listens on port 8080 by default and serves the website files from
the repo root. Open http://localhost:8080 in a browser.

Environment variables:

| Name       | Default              | What it does                    |
|------------|----------------------|---------------------------------|
| PORT       | 8080                 | Port to listen on               |
| STATIC_DIR | repo root next to the binary | Folder with index.html etc |
| QUOTES_DB  | ./quotes.db          | SQLite file for quote requests  |

Example with a custom port:

```
PORT=3000 ./te_server
```

## API

### GET /api/services

Returns the four service categories with their items.

```
curl http://localhost:8080/api/services
```

### POST /api/quote

Submit a quote request as JSON. `name`, `phone`, and `service` are required.
`email` is optional but must look like an email when given. `details` is
optional. Returns the new quote id.

```
curl -X POST http://localhost:8080/api/quote \
  -H "Content-Type: application/json" \
  -d '{"name":"Test Customer","phone":"850-555-0100","email":"test@example.com","service":"Pressure washing","details":"Driveway and sidewalk"}'
```

Success: `{"ok":true,"id":1}` with HTTP 200.
Bad input: `{"ok":false,"error":"phone is required"}` with HTTP 400.

### GET /api/quotes

Lists every stored quote, newest first. This is a simple owner view. Before
real customer data flows through it, add a login or secret token check on
this route. See the NOTE in `src/main.cpp`.

```
curl http://localhost:8080/api/quotes
```

## Run on AWS EC2

1. In the AWS console, launch an Ubuntu 22.04 or 24.04 instance
   (a t3.micro on the free tier is plenty).
2. In the security group step, add an inbound rule: Type HTTP, port 80,
   source 0.0.0.0/0. Without this, browsers cannot reach the site.
3. Expand "Advanced details" and paste the whole contents of
   `backend/scripts/ec2-user-data.sh` into the User data box.
4. Launch. On first boot the script installs the compiler, clones this
   repo to /opt/te, builds the server, and starts it with systemd on
   port 80.
5. Open http://YOUR-INSTANCE-PUBLIC-IP in a browser.

Check the server later with:

```
ssh -i your-key.pem ubuntu@YOUR-INSTANCE-PUBLIC-IP
sudo systemctl status te-backend
```

To ship new code: `git push` from your machine, then on the instance
`git -C /opt/te pull`, rebuild, and `sudo systemctl restart te-backend`.

## For Keidran: how this maps to class

* `Service` and `QuoteRequest` are plain classes with private data,
  constructors, and getters, the same shape as your COP 3330 work.
* `QuoteStore` owns a resource (the database handle) and deletes its copy
  operations, which is RAII, the same idea as managing memory yourself.
* Routes are just functions tied to URL paths. Crow handles the sockets
  and threads; your code handles the logic.
* The mutex in `QuoteStore` is why: Crow runs requests on many threads,
  and one SQLite connection cannot be used by two threads at once.
