# Cachify - Redis-like in-memory KV store

Build:
  $ make

Run server (default port 6379):
  $ ./cachify_server 6379

Run client:
  $ ./cachify_client 127.0.0.1 6379

Protocol:
  SET key value [ttl_seconds]
  GET key
  DEL key
  SIZE
  PING
  QUIT

Responses:
  +OK\n   for success
  -ERR <message>\n  for errors
  $<len>\n<value>\n for GET found
  $-1\n for GET not found
  :<number>\n for integer reply (SIZE)
