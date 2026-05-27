# Chat Project — Full Implementation Plan

## Context

We are building the **Data Communication final project**: a LAN multi-user chat
application. The high-level requirements come from
\[Chat Project/project\_ref.txt](Chat Project/project\_ref.txt) and the
architecture diagram the lecturer presented in class:

* **TCP** for all control traffic between client and server
(register / login / logout / create / join / leave group).
* **UDP multicast** for the actual group chat messages — server is not in the
path once a group is joined.
* Two client screens (auth screen → groups screen).
* Per joined group the client spawns **two extra terminal windows**
(sender + receiver) via `system("gnome-terminal …")`. Child window PIDs are
returned to the client via a **SysV message queue** so the client can
`kill()` them on leave/logout.
* A group with zero members auto-closes; its multicast (ip,port) returns to
a free-pool queue on the server.
* Must use the course **ADT library** (HashMap, Queue, gen\_dlist, etc.). No
ad-hoc data structures.

User \& partner constraints decided in conversation:

* Build the **UI sooner** so end-to-end flows are demoable early.
* **One** Makefile at the project root.
* Compile and run under **WSL/Linux** (POSIX sockets, SysV IPC, gnome-terminal).

The wire format is TLV — `\[T:1]\[L:1]\[payload:L]` — exactly as the lecturer
drew. A first cut of \[Chat Project/protocol.h](Chat Project/protocol.h) is
already in the repo; this plan keeps it as-is (see "Protocol notes" below
for the one adjustment we'll make during implementation).

\---

## Architecture overview

Mirrors the lecturer's block diagram. Three layers per side:

```
                          ┌──────────┐                ┌──────────┐
                          │ Server   │                │ Client   │
                          │  Main    │                │  Main    │
                          └────┬─────┘                └─────┬────┘
                               │                            │
                          ┌────▼─────┐                ┌─────▼────┐
                          │ Server   │ ← Protocol →   │ Client   │
                          │  Mng     │   (TLV codec)  │  Mng     │
                          └─┬──┬──┬──┘                └─┬──┬──┬──┘
                            │  │  │                     │  │  │
            ┌───────────────┘  │  └────────────┐        │  │  └──── UI
            │                  │               │        │  │
       ┌────▼────┐      ┌──────▼────┐    ┌─────▼───┐    │  └────── Client Groups Mng
       │ User    │      │ Server    │    │ Group   │    │              (owns msgq +
       │ Mng     │      │ Net (TCP) │    │ Mng     │    │               child PIDs)
       └────┬────┘      └─────▲─────┘    └────┬────┘    │
            │                 │                │        │
       User Hash       Comm-Link (TCP)    Group Hash   Client Net (TCP)
                                          + Free MC      ▲
                                            IP Queue     │
                                                     ────┘
```

Four binaries total (each its own `main`):

1. **server** — long-running TCP server, owns user/group state.
2. **client** — TCP client + the two UI screens.
3. **chat\_sender** — small program that reads stdin, sends to a multicast
group. Lives in its own gnome-terminal window.
4. **chat\_receiver** — small program that joins a multicast group, prints
received messages. Lives in its own gnome-terminal window.

`chat\_sender` / `chat\_receiver` are spawned by the **client** (not the server)
via `system("gnome-terminal -- ./chat\_sender …")`. On startup each sends its
`getpid()` back to the parent client through a **SysV message queue** with a
key derived from the client's own PID. The client stores those PIDs in
**Client Groups Mng** so it can `kill()` them on leave/logout.

\---

## File layout

```
Chat Project/
├── Makefile                       # one root Makefile, four targets
├── project\_ref.txt                # spec (already there)
│
├── shared/
│   ├── protocol.h                 # already drafted (TLV opcodes + status)
│   ├── protocol.c                 # encoder/decoder bodies
│   └── adt/                       # course ADTs copied/linked in
│       ├── HashMap.h  HashMap.c   # ← .c MUST be implemented (lecturer omitted)
│       ├── genqueue.h genqueue.c  # copy from Lesson8/HW\_stack\_queue
│       └── gen\_dlist.h gen\_dlist.c# copy from Lesson11
│
├── server/
│   ├── server\_main.c              # arg parsing, init, main loop, shutdown
│   ├── server\_mng.h  server\_mng.c # dispatcher: opcode → handler
│   ├── server\_net.h  server\_net.c # select() loop, accept/recv/send (TCP)
│   ├── user.h                     # User struct
│   ├── user\_mng.h    user\_mng.c   # User HashMap + sock\_fd→User secondary index
│   ├── group.h                    # Group struct
│   ├── group\_mng.h   group\_mng.c  # Group HashMap, counter logic
│   └── free\_mc\_queue.h free\_mc\_queue.c  # Queue<mcast\_pair\_t>
│
└── client/
    ├── client\_main.c              # creates Client Mng, drives UI loop
    ├── client\_mng.h  client\_mng.c # state machine, calls Protocol + Client Net
    ├── client\_net.h  client\_net.c # connect/send/recv (TCP, blocking + select)
    ├── client\_groups\_mng.h .c     # owns msgq + map<group → (sender\_pid, recv\_pid)>
    ├── ui.h          ui.c         # the two text-mode screens
    ├── chat\_sender.c              # standalone: UDP multicast sender
    └── chat\_receiver.c            # standalone: UDP multicast receiver
```

**Note on ADT placement.** We need `HashMap`, `genqueue`, and `gen\_dlist`
from elsewhere in the repo. Copying them into `shared/adt/` keeps the project
self-contained and means the Makefile doesn't have to reach across folders
with `../`. If we'd rather symlink (cleaner) we can — open question below.

\---

## Protocol notes

\[Chat Project/protocol.h](Chat Project/protocol.h) is already drafted. We're
keeping it essentially as-is, with one small tightening during
implementation:

* **`L` stays 1 byte** (max 255-byte payload). All our messages — username +
password, group name + status + mcast IP + port — fit comfortably under
255 bytes. Matches the lecturer's whiteboard exactly.
* **Multicast IP is sent as a string** in `JOIN\_GROUP\_REP` /
`CREATE\_GROUP\_REP` (e.g. `"239.1.1.7"`). Trades 5–6 wire bytes for being
able to grep the network capture and pass straight into `inet\_addr()`.
* **Port is 2 bytes big-endian** inside its own `\[L=2]\[hi]\[lo]` sub-TLV so
the parser stays uniform.
* **Status replies use `\[L=1]\[code]`** including `OP\_REG\_REP`, `OP\_LOGIN\_REP`,
`OP\_LOGOUT\_REP`, `OP\_LEAVE\_GROUP\_REP`, and the **failure** form of
`CREATE\_GROUP\_REP` / `JOIN\_GROUP\_REP`. Success form of those two carries
the mcast (ip, port).

If during implementation we find a message that won't fit in 255 bytes (we
don't expect any), we widen `L` to 2 bytes everywhere and update both encoders.

\---

## Critical interfaces (sketch)

The signatures below are not final — they're the shape each `.h` should
have, so we can split the work cleanly between us.

### `shared/protocol.h` (already drafted)

Already defines `ChatOpcode`, `ChatStatus`, and the encode/decode functions.
Implementation goes in `shared/protocol.c`.

### `server/user.h`

```c
typedef enum { USER\_INACTIVE, USER\_ACTIVE } UserState;

typedef struct User {
    char       name\[CHAT\_MAX\_USERNAME\_LEN + 1];
    char       password\[CHAT\_MAX\_PASSWORD\_LEN + 1];  /\* hash later if we add encryption \*/
    UserState  state;
    int        socket\_fd;       /\* valid only while ACTIVE; -1 otherwise \*/
    List\*      joined\_groups;   /\* gen\_dlist of (char\* group\_name)         \*/
} User;
```

Storing `joined\_groups` on the user is the back-reference the diagram
doesn't show but the spec needs: on `logout`, we walk this list to remove
the user from every group in O(k) instead of O(num\_groups).

### `server/user\_mng.h`

```c
typedef struct UserMng UserMng;

UserMng\* user\_mng\_create(void);
void     user\_mng\_destroy(UserMng\*\* pm);

ChatStatus user\_mng\_register(UserMng\* m, const char\* name, const char\* pass);
ChatStatus user\_mng\_login   (UserMng\* m, const char\* name, const char\* pass, int sockfd, User\*\* out);
ChatStatus user\_mng\_logout  (UserMng\* m, int sockfd, User\*\* out);

User\*      user\_mng\_find\_by\_name (UserMng\* m, const char\* name);
User\*      user\_mng\_find\_by\_sock (UserMng\* m, int sockfd);  /\* O(1) via 2nd index \*/
```

Internally: two `HashMap\*` — `by\_name` (key = `char\*`, owns the User) and
`by\_sock` (key = `int\*`, points at the same User, doesn't own it).

### `server/group.h`

```c
typedef struct Group {
    char     name\[CHAT\_MAX\_GROUPNAME\_LEN + 1];
    char     mcast\_ip\[CHAT\_MAX\_IP\_STR\_LEN];
    uint16\_t mcast\_port;
    size\_t   member\_count;     /\* spec: when this hits 0, the group dies   \*/
} Group;
```

No member list — per spec, the server only keeps a counter.

### `server/group\_mng.h`

```c
typedef struct GroupMng GroupMng;

GroupMng\* group\_mng\_create(FreeMcQueue\* pool);
void      group\_mng\_destroy(GroupMng\*\* pm);

ChatStatus group\_mng\_create\_group(GroupMng\* m, const char\* gname, Group\*\* out);
ChatStatus group\_mng\_join        (GroupMng\* m, const char\* gname, Group\*\* out);
ChatStatus group\_mng\_leave       (GroupMng\* m, const char\* gname);   /\* may destroy if count→0 \*/
Group\*     group\_mng\_find        (GroupMng\* m, const char\* gname);
```

### `server/free\_mc\_queue.h`

```c
typedef struct { char ip\[CHAT\_MAX\_IP\_STR\_LEN]; uint16\_t port; } McastPair;
typedef struct FreeMcQueue FreeMcQueue;

FreeMcQueue\* fmq\_create (size\_t capacity);   /\* pre-fills with 239.1.1.1:6000 … \*/
void         fmq\_destroy(FreeMcQueue\*\* pq);

int  fmq\_take   (FreeMcQueue\* q, McastPair\* out);   /\* 0 = ok, -1 = empty \*/
int  fmq\_return (FreeMcQueue\* q, McastPair pair);
```

Backed by the course `Queue` ADT
([Lesson8/HW\_stack\_queue/genqueue.h](Lesson8/HW_stack_queue/genqueue.h)).

### `server/server\_net.h`

Owns the `select()` loop. Calls a single callback into `server\_mng` whenever
a complete TLV message has arrived on a socket. Mostly a refactor of
[Networks/tcp\_server.c](Networks/tcp_server.c) into a module:

```c
typedef int (\*OnMessageFn)(int sockfd, const uint8\_t\* msg, size\_t len, void\* ctx);
typedef void(\*OnDisconnectFn)(int sockfd, void\* ctx);

int server\_net\_run(uint16\_t port,
                   OnMessageFn on\_msg,
                   OnDisconnectFn on\_disc,
                   void\* ctx);
```

Buffering note: TCP can fragment, so `server\_net` must accumulate bytes per
socket and only invoke `on\_msg` once `\[T]\[L]` + L bytes have arrived.

### `server/server\_mng.h`

The brain. Owns `UserMng`, `GroupMng`, `FreeMcQueue`. One function per
opcode, plus the master dispatcher:

```c
int server\_mng\_handle(int sockfd, const uint8\_t\* msg, size\_t len, void\* ctx);
void server\_mng\_on\_disconnect(int sockfd, void\* ctx);   /\* implicit logout \*/
```

### `client/client\_net.h`

Mirror of `server\_net` but for one socket. Blocking `connect`, then non-blocking

* `select()` for the steady state. Mostly a refactor of
[Networks/tcp\_client.c](Networks/tcp_client.c).

### `client/client\_mng.h`

```c
typedef struct ClientMng ClientMng;

ClientMng\* client\_mng\_create(const char\* server\_ip, uint16\_t port);
void       client\_mng\_destroy(ClientMng\*\* pm);

ChatStatus client\_mng\_register   (ClientMng\* m, const char\* user, const char\* pass);
ChatStatus client\_mng\_login      (ClientMng\* m, const char\* user, const char\* pass);
ChatStatus client\_mng\_logout     (ClientMng\* m);
ChatStatus client\_mng\_create\_grp (ClientMng\* m, const char\* gname);
ChatStatus client\_mng\_join\_grp   (ClientMng\* m, const char\* gname);
ChatStatus client\_mng\_leave\_grp  (ClientMng\* m, const char\* gname);
```

Each call is **synchronous**: send the TLV request, block on a reply, return
the status. The UI is single-threaded so this is fine.

### `client/client\_groups\_mng.h`

Holds, per joined group: the `(sender\_pid, receiver\_pid)` returned via the
SysV message queue. Owns the queue. On leave/logout walks the entries and
calls `kill(pid, SIGTERM)` on each, then frees its bookkeeping.

### `client/ui.h`

```c
void ui\_run(ClientMng\* m);   /\* shows screen 1, transitions to screen 2 after login \*/
```

\---

## Build order (UI sooner, three demo checkpoints)

Numbered phases. Each ends with a runnable demo we can record and submit.

### Phase 0 — Infra (½ day)

* Root **Makefile** with `server`, `client`, `chat\_sender`, `chat\_receiver`,
`clean`, `all`.
* Copy ADTs into `shared/adt/` (HashMap, genqueue, gen\_dlist) and verify they
compile standalone.
* **Implement `HashMap.c`** (lecturer left it as exercise — separate chaining
with linked-list buckets, nearest-prime capacity, lazy bucket allocation).
* Implement `shared/protocol.c` (the encoders/decoders declared in
\[Chat Project/protocol.h](Chat Project/protocol.h)).
* Write a tiny `tests/test\_protocol.c` that round-trips every message type.
(Not required by spec but cheap insurance.)

### Phase 1 — Skeleton server + skeleton client (1 day)

* `server\_net.c` (refactor of tcp\_server.c into a module with a callback).
* `server\_mng.c` with **stub** handlers — every opcode replies `ST\_OK`.
* `client\_net.c`, `client\_mng.c` with the six request functions.
* `ui.c` with **both screens** working: menus draw, prompts read input,
client sends real TLV requests and prints replies.

**✅ Demo #1**: launch server, launch two clients, each can navigate
through "register → login → create group → leave group → logout → exit".
Replies are all stub `OK`, but the UI flow is complete.

### Phase 2 — Real user management (½ day)

* `user.h` + `user\_mng.c` using the HashMap from Phase 0.
* Wire `server\_mng\_handle\_register` / `\_login` / `\_logout` to real
`user\_mng\_\*` calls.
* Handle implicit logout on socket disconnect (`server\_mng\_on\_disconnect`).

**✅ Demo #2**: register two users, log in, log out, log back in. Duplicate
usernames rejected. Wrong passwords rejected. Killing client = server marks
user inactive.

### Phase 3 — Real group management (1 day)

* `free\_mc\_queue.c` (Queue ADT, pre-fill on startup).
* `group.h` + `group\_mng.c` (HashMap of groups, counter logic, auto-destroy
on counter==0 and return pair to queue).
* Wire `server\_mng\_handle\_create\_group` / `\_join` / `\_leave`. Reply includes
the assigned multicast IP+port.
* On the client side, just **print** the multicast info in the UI for now —
don't spawn windows yet.

**✅ Demo #3**: two clients create/join/leave the same group, see the same
mcast (ip, port) returned. Server logs show counter rising/falling and
groups getting reclaimed at zero.

### Phase 4 — Multicast chat windows (1 day)

* `chat\_sender.c`: opens UDP socket, sets `IP\_MULTICAST\_TTL`, sends one
multicast datagram per line of stdin to `(argv\[1] ip, argv\[2] port)`.
* `chat\_receiver.c`: opens UDP socket, `bind()` to the port, joins the
multicast group via `setsockopt(IPPROTO\_IP, IP\_ADD\_MEMBERSHIP, …)`,
prints every datagram.
* Test the two binaries by hand from two terminals before any client
integration.

**✅ Mini-demo**: run two `chat\_receiver` and one `chat\_sender` manually,
confirm both receivers print every line.

### Phase 5 — Client groups manager + window spawning (1 day)

* `client\_groups\_mng.c`: create a SysV message queue at client startup
(`ftok` on the client's own PID, `msgget` with `IPC\_CREAT`).
* On successful `join`/`create`, the client `system()`-spawns:

&#x20;   ```
    gnome-terminal -- ./chat\_sender   <ip> <port> <client\_msqid>
    gnome-terminal -- ./chat\_receiver <ip> <port> <client\_msqid>
    ```

* `chat\_sender`/`chat\_receiver` are extended to send their `getpid()` to
the queue on startup. Client reads two messages, stores both PIDs against
the group name.
* On `leave`/`logout`, walk the joined groups and `kill(pid, SIGTERM)` each
PID, then `msgctl(IPC\_RMID)` the queue on shutdown.

**✅ Demo #4 — full system**: two clients log in, both join "general", both
get two chat windows each, they can type and see each other's messages in
real time. Leaving a group closes both windows. Logging out closes all.

### Phase 6 — Polish (½ day)

* Robust error paths (partial sends, malformed TLV, queue exhausted).
* Memory cleanup on every exit path — run under `valgrind` and fix leaks.
* README with build + run instructions.
* **Optional**: simple Caesar/XOR encryption on multicast payloads (spec
bonus item) — can skip if time-constrained.

\---

## Work split (two of us)

A reasonable parallel split, given the modules are loosely coupled:

|Partner A (server side)|Partner B (client + multicast)|
|-|-|
|HashMap.c|protocol.c|
|server\_net, server\_mng|client\_net, client\_mng|
|user\_mng + user.h|ui.c (both screens)|
|group\_mng + group.h|chat\_sender.c, chat\_receiver.c|
|free\_mc\_queue.c|client\_groups\_mng.c + msgq plumbing|

Both agree on `protocol.h` first (already drafted), then `protocol.c` —
that's the contract. After that the two halves can be developed and tested
independently against the protocol.

\---

## Critical files to create / modify

* **Modify**: \[Chat Project/protocol.h](Chat Project/protocol.h) — only if we
hit a 255-byte payload (we don't expect to).
* **Create**: everything under the file layout above.
* **Reuse without modification**: the ADTs in [Hash/HashMap.h](Hash/HashMap.h)
(header), [Lesson8/HW\_stack\_queue/genqueue.h](Lesson8/HW_stack_queue/genqueue.h)

  * `.c`, [Lesson11/gen\_dlist.h](Lesson11/gen_dlist.h) + `.c`.
* **Reuse as scaffolding**: [Networks/tcp\_server.c](Networks/tcp_server.c)
and [Networks/tcp\_client.c](Networks/tcp_client.c) — copy the
`socket/bind/listen/select` skeleton into `server\_net.c` and `client\_net.c`,
then strip the echo behaviour and add per-socket receive buffers + the
`on\_msg` / `on\_disc` callbacks.

\---

## Open questions to settle with partner

These are intentionally NOT answered in the plan — they need a joint call:

1. **Copy or symlink the ADTs into `shared/adt/`?** Symlinks are cleaner
(changes propagate), copies are safer (we control them). Recommend copy.
2. **Where do `chat\_sender` and `chat\_receiver` live in the file layout?**
I put them under `client/`. Could also be top-level since they're
independent binaries.
3. **How do we synchronise the message-queue key?** Simplest is the client's
own PID — pass it as a CLI arg to the spawned windows, both sides
`ftok("/tmp", client\_pid\_low\_byte)`. Open to alternatives.
4. **Encryption bonus**: do it or skip it? Adds maybe half a day; not on
the critical path.
5. **Improved UI bonus**: stay text-mode, or attempt `ncurses`? Strongly
recommend staying text-mode and putting effort into robustness instead.

\---

## Verification plan

End-to-end smoke test we'll run after each phase, from a single WSL shell:

```bash
# build everything
cd "/mnt/c/GIT/Embedded\_course/Chat Project"
make clean \&\& make all

# terminal 1
./server

# terminal 2
./client 127.0.0.1

# terminal 3
./client 127.0.0.1
```

Per-phase checks:

* **Phase 0**: `make all` produces 4 binaries with no warnings (`-Wall -Wextra -Werror`). `./tests/test\_protocol` passes.
* **Phase 1**: client menus render, navigating any path doesn't crash either
process, server logs show every TLV opcode received.
* **Phase 2**: register a name twice → second attempt rejected. Wrong
password → rejected. Two clients can be logged in simultaneously.
`Ctrl-C` a client → server prints "implicit logout of user X".
* **Phase 3**: create "g1" from client A → A is auto-joined and gets a
multicast (ip, port). B joins "g1" → gets the same (ip, port). A leaves,
B leaves → server log says "group g1 closed, pair returned to pool".
* **Phase 4**: standalone `./chat\_sender 239.1.1.1 6000` + two
`./chat\_receiver 239.1.1.1 6000` echo correctly.
* **Phase 5**: each client gets two new gnome-terminal windows on join,
messages typed in A's sender window appear in B's receiver window and
vice versa. Leaving the group closes both of A's windows.
* **Phase 6**: `valgrind --leak-check=full ./server` reports no leaks after
a full join/leave/logout cycle. Same for `./client`.

Submission deliverable: tarball of `Chat Project/` plus a 1-page README
mapping the lecturer's diagram boxes to file names.

