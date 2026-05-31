# Chat Project — LAN Multi-User Chat

Data Communication final project: a registered, real-time, multi-user chat
application for a LAN.

* **TCP** carries all control traffic (register / login / logout /
  create / join / leave group) between client and server.
* **UDP multicast** carries the actual group chat messages — once a client
  has joined a group the server is no longer in the message path.
* The client runs two text screens (auth → groups). For every group it
  joins it spawns **two `gnome-terminal` windows** (one to send, one to
  receive); those child processes report their PIDs back over a **SysV
  message queue** so the client can `kill()` them on leave/logout.
* A group with zero members closes automatically and its multicast
  `(ip, port)` returns to a free pool on the server.

The project uses the course **ADT library** (HashMap, generic Queue,
generic doubly-linked list) — no ad-hoc data structures.

---

## Requirements

* **Linux** (or **WSL** on Windows). The app uses POSIX sockets, SysV IPC,
  and `gnome-terminal`, so it does not build or run natively on Windows.
* `gcc` and `make`.
* `gnome-terminal` (for the chat windows in the full client flow). On a
  headless box you can still test the multicast tools by hand — see
  *Manual multicast test* below.

---

## Build

From the project root:

```bash
make clean && make all
```

This produces four binaries in `bin/`:

| Binary               | Role                                                        |
| -------------------- | ----------------------------------------------------------- |
| `bin/server`         | Long-running TCP server; owns all user and group state.     |
| `bin/client`         | TCP client + the two text UI screens.                       |
| `bin/chat_sender`    | UDP multicast **sender** window (reads stdin → multicast).  |
| `bin/chat_receiver`  | UDP multicast **receiver** window (multicast → stdout).     |

The build is warning-clean under `-Wall -Wextra -Werror`.

Extra developer targets:

```bash
make test_protocol     # build + then ./bin/test_protocol  (TLV round-trip tests)
make mock_server       # throwaway server for exercising the auth slice
```

---

## Run

The server listens on TCP port **5000** (`CHAT_TCP_PORT`). The client
connects to that port; the chat windows use multicast addresses handed out
by the server starting at **239.1.1.1:6000**.

Open three terminals on the same host (or LAN):

```bash
# terminal 1 — server
./bin/server

# terminal 2 — first client (defaults to 127.0.0.1)
./bin/client

# terminal 3 — second client, explicit server IP
./bin/client 127.0.0.1
```

`./bin/client [server_ip]` — `server_ip` defaults to `127.0.0.1` if omitted.
Stop the server with `Ctrl-C`; it shuts down gracefully and frees all state.

### Using the client

**Screen 1 — authentication**

```
=== Chat ===
  1) register
  2) login
  3) exit
```

1. **register** — prompts for username + password. Usernames must be unique;
   a duplicate is rejected.
2. **login** — prompts for username + password. On success you move to
   screen 2; wrong password / unknown user is rejected.
3. **exit** — closes the connection, frees everything, quits.

**Screen 2 — groups** (shown after a successful login)

```
=== Groups (logged in as 'alice') ===
  1) create group
  2) join group
  3) leave group
  4) logout
```

1. **create group** — enter a new group name. The server opens the group,
   auto-joins you, and returns its multicast `(ip, port)`. The client then
   opens your two chat windows for that group.
2. **join group** — enter an existing group name. The server returns the
   same multicast `(ip, port)`, and the client opens your two chat windows.
3. **leave group** — enter a joined group name. The client closes that
   group's two chat windows; if you were the last member the server closes
   the group and reclaims its multicast pair.
4. **logout** — the server drops you from every group and marks you
   inactive; the client closes all chat windows and returns to screen 1.

**Chatting**: in a group you get two windows. Type a line in the **sender**
window and press Enter — every member's **receiver** window prints it.
Press `Ctrl-D` in a sender window to stop it (the client also kills both
windows automatically on leave/logout).

### Manual multicast test (no client/server needed)

You can exercise the multicast tools directly — handy on a headless box:

```bash
# two receivers (different terminals), same group+port
./bin/chat_receiver 239.1.1.1 6000
./bin/chat_receiver 239.1.1.1 6000

# one sender — anything you type appears in both receivers
./bin/chat_sender 239.1.1.1 6000
```

`chat_sender` / `chat_receiver` take an optional third argument, a SysV
message-queue id; the client passes it so the children can report their
PIDs. Run by hand you simply omit it.

---

## Architecture → file map

Mirrors the lecturer's block diagram. Four binaries, three layers per side.

```
            ┌──────────┐                       ┌──────────┐
            │ Server   │                       │ Client   │
            │  Main    │  server_main.c        │  Main    │  client_main.c
            └────┬─────┘                       └────┬─────┘
                 │                                  │
            ┌────▼─────┐      ← Protocol →     ┌────▼─────┐
            │ Server   │      (TLV codec)      │ Client   │
            │  Mng     │  server_mng.c         │  Mng     │  client_mng.c
            └─┬──┬──┬──┘                       └─┬──┬──┬──┘
              │  │  │                            │  │  └──── UI            ui.c
   ┌──────────┘  │  └────────────┐              │  └─────── Client Groups  client_groups_mng.c
   │             │               │              │            Mng (msgq + child PIDs)
┌──▼───┐   ┌─────▼────┐   ┌──────▼──┐           └────────── Client Net     client_net.c
│ User │   │ Server   │   │ Group   │
│ Mng  │   │ Net(TCP) │   │ Mng     │
└──┬───┘   └────┬─────┘   └────┬────┘
   │            │              │
User Hash   Comm-Link     Group Hash +
(HashMap)    (TCP)        Free MC IP Queue
```

### Shared (the protocol contract + course ADTs)

| Box / concern        | File(s)                                              |
| -------------------- | ---------------------------------------------------- |
| Protocol (TLV codec) | `shared/protocol.h`, `shared/protocol.c`             |
| HashMap ADT          | `shared/adt/HashMap.h`, `shared/adt/HashMap.c`       |
| Generic Queue ADT    | `shared/adt/genqueue.h`, `shared/adt/genqueue.c`     |
| Generic dlist ADT    | `shared/adt/gen_dlist.h`, `shared/adt/gen_dlist.c`   |

### Server side

| Box                         | File(s)                                      |
| --------------------------- | -------------------------------------------- |
| Server Main                 | `server/server_main.c`                       |
| Server Mng (opcode → handler) | `server/server_mng.c/.h`                   |
| Server Net (TCP `select`)   | `server/server_net.c/.h`                     |
| User Mng (+ User Hash)       | `server/user_mng.c/.h`, `server/user.h`     |
| Group Mng (+ Group Hash)     | `server/group_mng.c/.h`, `server/group.h`   |
| Free MC IP Queue            | `server/free_mc_queue.c/.h`                  |
| Hashing helpers             | `server/hash_utils.c/.h`                     |

### Client side

| Box                              | File(s)                            |
| -------------------------------- | ---------------------------------- |
| Client Main                      | `client/client_main.c`             |
| Client Mng (state machine)       | `client/client_mng.c/.h`           |
| Client Net (TCP)                 | `client/client_net.c/.h`           |
| Client Groups Mng (msgq + PIDs)  | `client/client_groups_mng.c/.h`    |
| UI (two screens)                 | `client/ui.c/.h`                   |
| Multicast sender window          | `client/chat_sender.c`             |
| Multicast receiver window        | `client/chat_receiver.c`           |
| Child→parent msgq message format | `client/child_msg.h`               |

### Tests

| Purpose                          | File                     |
| -------------------------------- | ------------------------ |
| TLV protocol round-trip tests    | `tests/test_protocol.c`  |
| Auth-slice throwaway server      | `tests/mock_server.c`    |

---

## Protocol (wire format)

Control messages are **TLV** — a 1-byte Type, a 1-byte Length, then `L`
payload bytes. Variable sub-fields inside a payload use the same
`[len][value]` shape; multi-byte integers are big-endian.

```
+-------+-------+--------------------+
|   T   |   L   |   payload (L B)    |
+-------+-------+--------------------+
```

Example — `REG_REQ` for user `"abc"` / password `"123"`:

```
01 08   03 'a''b''c'   03 '1''2''3'
 T  L    L1  value      L2  value
```

Opcodes, status codes, and the exact payload of each message are defined in
[shared/protocol.h](shared/protocol.h); the encoders/decoders live in
[shared/protocol.c](shared/protocol.c) and are covered by
[tests/test_protocol.c](tests/test_protocol.c).
