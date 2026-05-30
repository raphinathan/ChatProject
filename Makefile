# =============================================================================
#  Chat Project - root Makefile
#  Final binaries: server, client, chat_sender, chat_receiver.
#  Today's auth slice also builds: mock_server, test_protocol (in tests/).
#  Build/run target: WSL/Linux (POSIX sockets, SysV IPC, gnome-terminal).
#
#  Usage:
#     make                 # build the four product binaries (alias: make all)
#     make client          # build one binary
#     make mock_server     # throwaway test server (auth slice)
#     make test_protocol   # protocol round-trip unit test
#     make clean           # remove build/ and bin/
#
#  Binaries land in bin/, intermediate objects in build/ (mirrors src tree).
# =============================================================================

CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -g -std=gnu11
CFLAGS  += -Ishared -Ishared/adt -Iserver -Iclient
LDFLAGS :=

BUILD := build
BIN   := bin

# ---- protocol codec: linked into every binary that touches the wire ---------
PROTOCOL_SRC := shared/protocol.c

# ---- course ADTs: server-only (client + multicast tools don't need them) ----
ADT_SRC := shared/adt/HashMap.c \
           shared/adt/genqueue.c \
           shared/adt/gen_dlist.c

# ---- per-binary sources -----------------------------------------------------
SERVER_SRC := server/server_main.c \
              server/server_mng.c \
              server/server_net.c \
              server/user_mng.c \
              server/group_mng.c \
              server/free_mc_queue.c \
              server/hash_utils.c

CLIENT_SRC := client/client_main.c \
              client/client_mng.c \
              client/client_net.c \
              client/client_groups_mng.c \
              client/ui.c

# client_groups_mng uses gen_dlist; link it only into the client binary.
CLIENT_ADT_SRC := shared/adt/gen_dlist.c

# chat_sender / chat_receiver are standalone UDP tools (no server/client deps).
SENDER_SRC   := client/chat_sender.c
RECEIVER_SRC := client/chat_receiver.c

# auth-slice test harnesses (disposable / dev-only)
MOCK_SRC := tests/mock_server.c
TEST_SRC := tests/test_protocol.c

# foo/bar.c -> build/foo/bar.o
obj = $(patsubst %.c,$(BUILD)/%.o,$(1))

PROTOCOL_OBJ := $(call obj,$(PROTOCOL_SRC))
ADT_OBJ      := $(call obj,$(ADT_SRC))
SERVER_OBJ   := $(call obj,$(SERVER_SRC))
CLIENT_OBJ   := $(call obj,$(CLIENT_SRC))
CLIENT_ADT_OBJ := $(call obj,$(CLIENT_ADT_SRC))
SENDER_OBJ   := $(call obj,$(SENDER_SRC))
RECEIVER_OBJ := $(call obj,$(RECEIVER_SRC))
MOCK_OBJ     := $(call obj,$(MOCK_SRC))
TEST_OBJ     := $(call obj,$(TEST_SRC))

# -----------------------------------------------------------------------------
.PHONY: all clean server client chat_sender chat_receiver mock_server test_protocol

all: server client chat_sender chat_receiver

# short-name aliases
server:        $(BIN)/server
client:        $(BIN)/client
chat_sender:   $(BIN)/chat_sender
chat_receiver: $(BIN)/chat_receiver
mock_server:   $(BIN)/mock_server
test_protocol: $(BIN)/test_protocol

$(BIN)/server: $(SERVER_OBJ) $(PROTOCOL_OBJ) $(ADT_OBJ)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN)/client: $(CLIENT_OBJ) $(PROTOCOL_OBJ) $(CLIENT_ADT_OBJ)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN)/chat_sender: $(SENDER_OBJ)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN)/chat_receiver: $(RECEIVER_OBJ)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN)/mock_server: $(MOCK_OBJ) $(PROTOCOL_OBJ)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN)/test_protocol: $(TEST_OBJ) $(PROTOCOL_OBJ)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# generic compile rule: any .c -> build/.../.o
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(BIN)
