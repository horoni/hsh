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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* But in fact its 1 less */
#define ARGS_SIZE 32
#define BUFFER_SIZE 1024

typedef struct {
  char **argv;
  int    argc;
  char   input[BUFFER_SIZE];
  int    last_status;
} shell_ctx;

struct Command {
  const char *name;
  void (*func)(shell_ctx *);
};

static void   free_argv    (shell_ctx *);

static int    find_in_path (const char *needle, char *out, size_t n);
static char **parse_args   (shell_ctx *);
static int    exec_builtins(shell_ctx *);
static int    try_exec_ext (shell_ctx *);
static void   exit_comm    (shell_ctx *);
static void   echo_comm    (shell_ctx *);
static void   type_comm    (shell_ctx *);
static void   pwd_comm     (shell_ctx *);
static void   cd_comm      (shell_ctx *);

struct Command comms[] = {
  {.name = "echo", .func = echo_comm},
  {.name = "type", .func = type_comm},
  {.name = "exit", .func = exit_comm},
  {.name = "pwd",  .func = pwd_comm },
  {.name = "cd",   .func = cd_comm  },
};

int main(void)
{
  shell_ctx ctx;
  memset(&ctx, 0, sizeof(ctx));

  setbuf(stdout, NULL);

  while(1) {
    printf("$ ");
    if (!fgets(ctx.input, sizeof(ctx.input), stdin))
      goto error;
    ctx.input[strlen(ctx.input)-1] = '\0';
    parse_args(&ctx);
    if (ctx.argc < 1)
      goto end_cyc;
    if (try_exec_ext(&ctx))
      printf("%s: command not found\n", ctx.argv[0]);
  end_cyc:
    free_argv(&ctx);
  }
  return 0;
error:
  (void)puts("fgets: error reading from stdin...\n");
  return 1;
}

static void free_argv(shell_ctx *ctx) {
  free(ctx->argv[ARGS_SIZE-1]);
  free(ctx->argv);
}

static char *strdup_(const char *str)
{
  char *new = NULL;

  if (!str)
    goto fail;
  new = malloc(strlen(str)+1);
  if (!new)
    goto fail;
  strcpy(new, str);
fail:
  return new;
}

static void exit_comm(shell_ctx *ctx)
{
  int exit_code = 0;
  if (ctx->argc > 1)
    exit_code = atoi(ctx->argv[1]);
  free_argv(ctx);
  exit(exit_code);
}

// TODO: support for '-n' argument
static void echo_comm(shell_ctx *ctx)
{
  for (int i = 1; i < ctx->argc; i++)
    printf("%s ", ctx->argv[i]);
  putchar('\n');
}

static void type_comm(shell_ctx *ctx)
{
  char finded[BUFFER_SIZE];
  for (int i = 0; i < sizeof(comms)/sizeof(struct Command); i++) {
    if (!strcmp(comms[i].name, ctx->argv[1])) {
      printf("%s is a shell builtin\n", ctx->argv[1]);
      return;
    }
  }
  if (!find_in_path(ctx->argv[1], finded, sizeof(finded)))
    printf("%s is %s\n", ctx->argv[1], finded);
  else
    printf("%s: not found\n", ctx->argv[1]);
}

static void pwd_comm(shell_ctx *ctx)
{
  char buf[BUFFER_SIZE];
  printf("%s\n", getcwd(buf, sizeof(buf)));
}

static void cd_comm(shell_ctx *ctx)
{
  char buf[BUFFER_SIZE];

  if (ctx->argc == 1) {
    chdir(getenv("HOME"));
    return;
  }

  if (ctx->argv[1][0] == '~')
    snprintf(buf, sizeof(buf), "%s%s", getenv("HOME"), ctx->argv[1]+1);
  else
    strcpy(buf, ctx->argv[1]);

  if (chdir(buf))
    printf("%s: No such file or directory\n", ctx->argv[1]);
}

static int exec_builtins(shell_ctx* ctx)
{
  for (int i = 0; i < sizeof(comms)/sizeof(struct Command); i++) {
    if (!strcmp(ctx->argv[0], comms[i].name)) {
      comms[i].func(ctx);
      return 0;
    }
  }
  return 1;
}

static int try_exec_ext(shell_ctx *ctx)
{
  char buf[BUFFER_SIZE] = {0};
  pid_t pid = 0;

  if (exec_builtins(ctx) == 0)
    return 0;
  if (find_in_path(ctx->argv[0], NULL, 0))
    return 1;

  pid = fork();

  if (pid == 0) { // child
    find_in_path(ctx->argv[0], buf, sizeof(buf));
    execvp(buf, ctx->argv);
  } else if (pid > 0) { // parent
    waitpid(pid, &ctx->last_status, 0);
  }

  return 0;
}

/**
 * If success returns pointer to argv
 */
static char **parse_args(shell_ctx *ctx)
{
  char *inp = (char*)strdup_(ctx->input);
  char **args = malloc(ARGS_SIZE * sizeof(char *));
  char *arg = strtok(inp, " ");
  int idx = 0;

  if (!args)
    goto end;
  memset(args, 0, ARGS_SIZE * sizeof(char *));

  while (arg != NULL && idx < ARGS_SIZE-1) {
    args[idx++] = arg;
    arg = strtok(NULL, " ");
  }

  ctx->argc = idx;
  ctx->argv = args;
  args[ARGS_SIZE-1] = inp;

end:
  return args;
}

static int find_in_path(const char *needle, char *out, size_t n)
{
  char path_to_check[BUFFER_SIZE] = {0};
  char *path = (char*)strdup_(getenv("PATH"));
  const char *dir = NULL;
  int need_size = 0;

  if (!path)
    goto error;

  dir = strtok(path, ":");
  while (dir != NULL) {
    snprintf(path_to_check, sizeof(path_to_check), "%s/%s", dir, needle);
    if (access(path_to_check, X_OK) == 0) {
      free(path);
      if (out != NULL) {
        need_size = strlen(path_to_check);
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

error:
  return -1;
}

