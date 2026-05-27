# =============================================================================
#  Chat Project - root Makefile
#  Four binaries: server, client, chat_sender, chat_receiver.
#  Build/run target: WSL/Linux (POSIX sockets, SysV IPC, gnome-terminal).
#
#  Usage:
#     make            # build everything (alias: make all)
#     make server     # build one binary
#     make clean      # remove build/ and bin/
#
#  Binaries land in bin/, intermediate objects in build/ (mirrors src tree).
# =============================================================================

CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -g -std=gnu11
CFLAGS  += -Ishared -Ishared/adt -Iserver -Iclient
LDFLAGS :=

BUILD := build
BIN   := bin

# ---- shared code linked into both server and client -------------------------
SHARED_SRC := shared/protocol.c \
              shared/adt/HashMap.c \
              shared/adt/genqueue.c \
              shared/adt/gen_dlist.c

# ---- per-binary sources -----------------------------------------------------
SERVER_SRC := server/server_main.c \
              server/server_mng.c \
              server/server_net.c \
              server/user_mng.c \
              server/group_mng.c \
              server/free_mc_queue.c

CLIENT_SRC := client/client_main.c \
              client/client_mng.c \
              client/client_net.c \
              client/client_groups_mng.c \
              client/ui.c

# chat_sender / chat_receiver are standalone UDP tools (no server/client deps).
SENDER_SRC   := client/chat_sender.c
RECEIVER_SRC := client/chat_receiver.c

# foo/bar.c -> build/foo/bar.o
obj = $(patsubst %.c,$(BUILD)/%.o,$(1))

SHARED_OBJ   := $(call obj,$(SHARED_SRC))
SERVER_OBJ   := $(call obj,$(SERVER_SRC))
CLIENT_OBJ   := $(call obj,$(CLIENT_SRC))
SENDER_OBJ   := $(call obj,$(SENDER_SRC))
RECEIVER_OBJ := $(call obj,$(RECEIVER_SRC))

# -----------------------------------------------------------------------------
.PHONY: all clean server client chat_sender chat_receiver

all: server client chat_sender chat_receiver

# short-name aliases
server:        $(BIN)/server
client:        $(BIN)/client
chat_sender:   $(BIN)/chat_sender
chat_receiver: $(BIN)/chat_receiver

$(BIN)/server: $(SERVER_OBJ) $(SHARED_OBJ)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN)/client: $(CLIENT_OBJ) $(SHARED_OBJ)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN)/chat_sender: $(SENDER_OBJ)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN)/chat_receiver: $(RECEIVER_OBJ)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# generic compile rule: any .c -> build/.../.o
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(BIN)
