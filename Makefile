PHONY   := all run clean valgrind
PROJECT := hsh

SRC_FILES := $(wildcard *.c)

OBJ_DIR   := obj
OBJ_FILES := $(addprefix $(OBJ_DIR)/,$(notdir $(SRC_FILES:.c=.o)))

CC       := gcc
WARNINGS := -Wall -Wextra -Wpedantic
CFLAGS   := $(WARNINGS) -std=c99

ifeq ($(MSAN),1)
$(info [SAN] MSan enabled)
CFLAGS  += -g -fPIE -fsanitize=memory -fno-omit-frame-pointer
LDFLAGS += -fPIE -pie -fsanitize=memory
else ifeq ($(ASAN),1)
$(info [SAN] ASan enabled)
CFLAGS  += -g -fsanitize=address,undefined,leak -fno-omit-frame-pointer
LDFLAGS += -fsanitize=address,undefined,leak
endif

RM    := rm -f
RMF   := rm -rf
RMDIR := rmdir

all: run

run: $(PROJECT)
	@./$(PROJECT)

$(PROJECT): $(OBJ_FILES)
	@$(CC) $^ -o $@

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@echo [CC] $@
	@$(CC) $^ $(CFLAGS) -c -o $@

$(OBJ_DIR):
	@mkdir -p $@

clean:
	@$(RM) $(OBJ_DIR)/*.o $(PROJECT)
	@$(RMDIR) $(OBJ_DIR)

valgrind: $(PROJECT)
	@valgrind ./$(PROJECT)

