/*
 * Copyright (c) 2025, horoni. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// TODO: Fix memory leak in `parse_args()` - `strdup()`

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

struct Command {
  const char *name;
  void (*func)(char**);
};

static char **parse_args(int *size);
static int find_in_path(const char *needle, char *out, size_t n);
static int exec_builtins(char **args);
static int try_exec_ext(char **args);
static void exit_comm(char **args);
static void echo_comm(char **args);
static void type_comm(char **args);
static void  pwd_comm(char **args);
static void   cd_comm(char **args);

struct Command comms[] = {
  {.name = "echo", .func = echo_comm},
  {.name = "type", .func = type_comm},
  {.name = "exit", .func = exit_comm},
  {.name = "pwd",  .func = pwd_comm },
  {.name = "cd",   .func = cd_comm  },
};

char input[1024];
int last_status = 0;
int argc =0;

int main(void) {
  char **args;
  // Flush after every printf
  setbuf(stdout, NULL);

  while(1) {
    printf("$ ");
    fgets(input, 1024, stdin);
    input[strlen(input)-1] = '\0';
    args = parse_args(&argc);
    if (argc == 0) continue;
    if (try_exec_ext(args))
      printf("%s: command not found\n", input);
    free(args);
  }
  return 0;
}

char *strdup_(char *str) {
  char *new = malloc(strlen(str)+1);
  strcpy(new, str);
  return new;
}

static void exit_comm(char **args) {
  if (argc >1) {
    int exi = atoi(args[1]);
    free(args);
    exit(exi);
  }
  else exit(0);
}
static void echo_comm(char **args) {
  printf("%s\n", input+5);
}
static void type_comm(char **args) {
  char finded[1024];
  for (int i = 0; i < sizeof(comms)/sizeof(struct Command); i++) {
    if (!strcmp(comms[i].name, args[1])) {
      printf("%s is a shell builtin\n", args[1]);
      return;
    }
  }
  if (!find_in_path(args[1], finded, 1024)) {
    printf("%s is %s\n", args[1], finded);
  } else printf("%s: not found\n", args[1]);
}
static void pwd_comm(char **args) {
  char buf[1024];
  printf("%s\n", getcwd(buf, 1024));
}
static void cd_comm(char **args) {
  if (argc ==1) {
    chdir(getenv("HOME"));
    return;
  }
  char buf[1024];
  if (args[1][0] == '~') {
    snprintf(buf, 1024, "%s%s", getenv("HOME"), args[1]+1);
  } else {
    strcpy(buf, args[1]);
  }
  if (chdir(buf)) {
    printf("%s: No such file or directory\n", args[1]);
  }
}

static int exec_builtins(char **args) {
  for (int i = 0; i < sizeof(comms)/sizeof(struct Command); i++) {
    if (!strcmp(args[0], comms[i].name)) {
      comms[i].func(args);
      return 0;
    }
  }
  return 1;
}

static int try_exec_ext(char **args) {
  if (exec_builtins(args) == 0) return 0;
  char buf[1024] = {0};
  if (!find_in_path(args[0], NULL, 0)) {
    pid_t pid = fork();
    if (pid == 0) { // child
      find_in_path(args[0], buf, 1024);
      if (execvp(buf, args) == -1); // error occured
    } else if (pid > 0) { // parent
      waitpid(pid, &last_status, 0);
    }
    return 0;
  }
  return 1;
}

static char **parse_args(int *size) {
  char *inp = (char*)strdup_(input);
  char **args = malloc(32 * sizeof(char *));
  char *arg = strtok(inp, " ");
  int idx = 0;
  while (arg != NULL) {
    args[idx++] = arg;
    arg = strtok(NULL, " ");
  }
  if (size != NULL) {
    *size = idx;
  }
  return args;
}

static int find_in_path(const char *needle, char *out, size_t n) {
  char path_to_check[1024] = {0};
  char *path = (char*)strdup_(getenv("PATH"));
  if (path != NULL) {
    char *dir = strtok(path, ":");
    while (dir != NULL) {
      snprintf(path_to_check, 1024, "%s/%s", dir, needle);
      if (access(path_to_check, X_OK) == 0) {
        free(path);
        if (out != NULL) {
          int need_size = strlen(path_to_check);
          if (need_size < n) {
            strcpy(out, path_to_check);
            return 0;
          }
          return need_size;
        }
        return 0;
      }
      dir = strtok(NULL, ":");
    }
    free(path);
  }
  return -1;
}
