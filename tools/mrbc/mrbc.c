#ifdef MRC_NO_STDIO
  #error mruby-bin-mrbc2 conflicts 'MRC_NO_STDIO' in your build configuration
#endif

#include <stdlib.h>
#include <string.h>
/*
 * mrb_xxx.h are in mruby-compiler2/include
 */
#include "mrc_common.h"
#include "mrc_irep.h"
#include "mrc_ccontext.h"
#include "mrc_dump.h"
#include "mrc_compile.h"
#include "mrc_pool.h"

/*
* We don't need `mrb_state *mrb` for mrbc executable
* while we do it when the parser is called in mruby runtime.
*/
typedef void mrb_state;

#define MRB NULL

#define RITEBIN_EXT ".mrb"
#define C_EXT       ".c"

#ifndef EXIT_SUCCESS
  #define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
  #define EXIT_FAILURE 1
#endif

struct mrc_args {
  const char *prog;
  const char *outfile;
  const char *initname;
  char **argv;
  int argc;
  int idx;
  mrc_bool dump_struct  : 1;
  mrc_bool check_syntax : 1;
  mrc_bool verbose      : 1;
  mrc_bool remove_lv    : 1;
  mrc_bool no_ext_ops   : 1;
  mrc_bool no_optimize  : 1;
  uint8_t flags         : 2;
};

static void
mrc_show_version(void)
{
  printf("mrbc %s\n", mrc_description());
}

static void
mrc_show_copyright(void)
{
  printf("Copyright (c) 2010-%s mruby developers\n", MRC_STRINGIZE(MRC_RELEASE_YEAR));
}

static void
usage(const char *name)
{
  static const char *const usage_msg[] = {
  "switches:",
  "-c           check syntax only",
  "-o<outfile>  place the output into <outfile>; required for multi-files",
  "-v           print version number, then turn on verbose mode",
  "-g           produce debugging information",
  "-B<symbol>   binary <symbol> output in C language format",
  "-S           dump C struct (requires -B)",
  "-s           define <symbol> as static variable",
  "--remove-lv  remove local variables",
  "--no-ext-ops prohibit using OP_EXTs",
  "--no-optimize disable peephole optimization",
  "--verbose    run at verbose mode",
  "--version    print the version",
  "--copyright  print the copyright",
  NULL
  };
  const char *const *p = usage_msg;

  printf("Usage: %s [switches] programfile...\n", name);
  while (*p)
    printf("  %s\n", *p++);
}

static char *
get_outfilename(char *infile, const char *ext)
{
  size_t ilen, flen, elen;
  char *outfile;
  char *p = NULL;

  ilen = strlen(infile);
  flen = ilen;
  if (*ext) {
    elen = strlen(ext);
    if ((p = strrchr(infile, '.'))) {
      ilen = p - infile;
    }
    flen = ilen + elen;
  }
  else {
    flen = ilen;
  }
  outfile = (char*)mrc_malloc(flen+1);
  memcpy(outfile, infile, ilen);
  outfile[ilen] = '\0';
  if (p) {
    memcpy(outfile+ilen, ext, elen);
    outfile[ilen + elen] = '\0';
  }

  return outfile;
}

static int
parse_args(int argc, char **argv, struct mrc_args *args)
{
  static const struct mrc_args args_zero = { 0 };
  int i;

  *args = args_zero;
  args->argc = argc;
  args->argv = argv;
  args->prog = argv[0];

  for (i=1; i<argc; i++) {
    if (argv[i][0] == '-') {
      switch ((argv[i])[1]) {
      case 'o':
        if (args->outfile) {
          fprintf(stderr, "%s: an output file is already specified. (%s)\n",
                  args->prog, args->outfile);
          return -1;
        }
        if (argv[i][2] == '\0' && argv[i+1]) {
          i++;
          args->outfile = get_outfilename(argv[i], "");
        }
        else {
          args->outfile = get_outfilename(argv[i] + 2, "");
        }
        break;
      case 'S':
        args->dump_struct = TRUE;
        break;
      case 'B':
        if (argv[i][2] == '\0' && argv[i+1]) {
          i++;
          args->initname = argv[i];
        }
        else {
          args->initname = argv[i]+2;
        }
        if (*args->initname == '\0') {
          fprintf(stderr, "%s: function name is not specified.\n", args->prog);
          return -1;
        }
        break;
      case 'c':
        args->check_syntax = TRUE;
        break;
      case 'v':
        if (!args->verbose) mrc_show_version();
        args->verbose = TRUE;
        break;
      case 'g':
        args->flags |= MRC_DUMP_DEBUG_INFO;
        break;
      case 's':
        args->flags |= MRC_DUMP_STATIC;
        break;
      case 'E':
      case 'e':
        fprintf(stderr, "%s: -e/-E option no longer needed.\n", args->prog);
        break;
      case 'h':
        return -1;
      case '-':
        if (argv[i][1] == '\n') {
          return i;
        }
        if (strcmp(argv[i] + 2, "version") == 0) {
          mrc_show_version();
          exit(EXIT_SUCCESS);
        }
        else if (strcmp(argv[i] + 2, "verbose") == 0) {
          args->verbose = TRUE;
          break;
        }
        else if (strcmp(argv[i] + 2, "copyright") == 0) {
          mrc_show_copyright();
          exit(EXIT_SUCCESS);
        }
        else if (strcmp(argv[i] + 2, "remove-lv") == 0) {
          args->remove_lv = TRUE;
          break;
        }
        else if (strcmp(argv[i] + 2, "no-ext-ops") == 0) {
          args->no_ext_ops = TRUE;
          break;
        }
        else if (strcmp(argv[i] + 2, "no-optimize") == 0) {
          args->no_optimize = TRUE;
          break;
        }
        return -1;
      default:
        return i;
      }
    }
    else {
      break;
    }
  }
  return i;
}

static void
cleanup(struct mrc_args *args)
{
  mrc_free((void*)args->outfile);
}

static ssize_t
input_files_length(struct mrc_args *args)
{
  ssize_t length = 0;
  FILE *file;
  int i;
  for (i = args->idx; i < args->argc; i++) {
    file = fopen(args->argv[i], "rb");
    if (file == NULL) {
      fprintf(stderr, "%s: cannot open program file. (%s)\n", args->prog, args->argv[i]);
      return -1;
    }
    fseek(file, 0, SEEK_END);
    length += ftell(file) + 1;
    fclose(file);
  }
  return length;
}

static uint8_t *
read_input_files(struct mrc_args *args, size_t length)
{
  int i;
  size_t pos = 0;
  size_t each_size;
  FILE *file;
  uint8_t *source = (uint8_t *)mrc_malloc(length);
  for (i = args->idx; i < args->argc; i++) {
    file = fopen(args->argv[i], "rb");
    fseek(file, 0, SEEK_END);
    each_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (fread(source + pos, sizeof(char), each_size, file) != each_size) {
      fprintf(stderr, "%s: cannot read program file. (%s)\n", args->prog, args->argv[i]);
      return NULL;
    }
    fclose(file);
    pos += each_size;
    if (length <= pos) {
      fprintf(stderr, "%s: insufficient memory allocated for input files. (%s)\n", args->prog, args->argv[i]);
      return NULL;
    }
    source[pos++] = '\n';
  }
  source[pos - 1] = '\0';
  return source;
}

#define INITIAL_BUF_SIZE 1024

static uint8_t *
read_from_stdin(ssize_t *result_length)
{
  *result_length = -1;
  uint8_t *buffer = mrc_malloc(INITIAL_BUF_SIZE);
  if (buffer == NULL) return NULL;

  int capacity = INITIAL_BUF_SIZE;
  size_t length = 0;

  while (1) {
    int ch = getchar();
    if (ch == EOF) {
      buffer[length] = '\0';
      *result_length = length;
      return buffer;
    }

    buffer[length] = (uint8_t)ch;
    length++;

    if (capacity <= length) {
      capacity *= 2;
      uint8_t *new_buffer = mrc_realloc(buffer, capacity);
      if (new_buffer == NULL) {
        mrc_free(buffer);
        return NULL;
      }
      buffer = new_buffer;
    }
  }
}

static mrc_irep *
load_file(mrc_ccontext *c, struct mrc_args *args)
{
  mrc_irep *irep;
  char *input = args->argv[args->idx];

  if (args->verbose) c->dump_result = TRUE;
  c->no_exec = TRUE;
  c->no_ext_ops = args->no_ext_ops;
  c->no_optimize = args->no_optimize;
  if (input[0] == '-' && input[1] == '\0') {
    /* stdin */
    ssize_t length;
    uint8_t *source;
    mrc_ccontext_filename(c, "(stdin)");
    source = read_from_stdin(&length);
    if (source == NULL) return NULL;
    irep = mrc_load_string_cxt(c, source, (size_t)length);
  }
  else if (args->idx == args->argc - 1) {
    /* single file */
    mrc_ccontext_filename(c, input);
    irep = mrc_load_file_cxt(c, input);
  }
  else {
    /* multiple files */
    ssize_t length;
    uint8_t *source;
    mrc_ccontext_filename(c, "(multiple files)");
    length = input_files_length(args);
    if (length < 0) return NULL;
    source = read_input_files(args, (size_t)length);
    irep = mrc_load_string_cxt(c, source, (size_t)length);
    mrc_free(source);
  }

  return irep;
}

static int
dump_file(mrc_ccontext *c, FILE *wfp, const char *outfile, const mrc_irep *irep, struct mrc_args *args)
{
  int n = MRC_DUMP_OK;

  if (args->remove_lv) {
    mrc_irep_remove_lv(c, (mrc_irep *)irep);
  }
  if (args->initname) {
    if (args->dump_struct) {
      n = mrc_dump_irep_cstruct(c, irep, args->flags, wfp, args->initname);
    }
    else {
      n = mrc_dump_irep_cfunc(c, irep, args->flags, wfp, args->initname);
    }
    if (n == MRC_DUMP_INVALID_ARGUMENT) {
      fprintf(stderr, "%s: invalid C language symbol name\n", args->initname);
    }
  }
  else {
    n = mrc_dump_irep_binary(c, irep, args->flags, wfp);
  }
  if (n != MRC_DUMP_OK) {
    fprintf(stderr, "%s: error in mrb dump (%s) %d\n", args->prog, outfile, n);
  }
  return n;
}

#if defined(MRC_PARSER_LRAMA)
#include "ruby.h"
// FIXME: workaround
int loglevel = 0;
#endif

int
main(int argc, char **argv)
{
  int n, result;
  struct mrc_args args;
  FILE *wfp;
  mrc_irep *irep;

  n = parse_args(argc, argv, &args);
  if (n < 0) {
    cleanup(&args);
    usage(argv[0]);
    return EXIT_FAILURE;
  }
  if (n == argc) {
    fprintf(stderr, "%s: no program file given\n", args.prog);
    return EXIT_FAILURE;
  }
  if (args.outfile == NULL && !args.check_syntax) {
    if (n + 1 == argc) {
      args.outfile = get_outfilename(argv[n], args.initname ? C_EXT : RITEBIN_EXT);
    }
    else {
      fprintf(stderr, "%s: output file should be specified to compile multiple files\n", args.prog);
      return EXIT_FAILURE;
    }
  }

  args.idx = n;
  mrc_ccontext *c = mrc_ccontext_new(MRB);
  irep = load_file(c, &args);

  if (args.check_syntax) {
    printf("%s:%s:Syntax OK\n", args.prog, argv[n]);
    cleanup(&args);
    mrc_irep_free(c, irep);
    return EXIT_SUCCESS;
  }

  if (irep == NULL){
    cleanup(&args);
    return EXIT_FAILURE;
  }
  if (args.outfile) {
    if (strcmp("-", args.outfile) == 0) {
      wfp = stdout;
    }
    else if ((wfp = fopen(args.outfile, "wb")) == NULL) {
      fprintf(stderr, "%s: cannot open output file:(%s)\n", args.prog, args.outfile);
      return EXIT_FAILURE;
    }
  }
  else {
    fputs("Output file is required\n", stderr);
    return EXIT_FAILURE;
  }
  result = dump_file(c, wfp, args.outfile, irep, &args);
  mrc_ccontext_free(c);
  fclose(wfp);
  cleanup(&args);
  mrc_irep_free(c, irep);

  if (result != MRC_DUMP_OK) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

void
mrb_init_mrblib(mrb_state *mrb)
{
}

#ifndef MRB_NO_GEMS
void
mrb_init_mrbgems(mrb_state *mrb)
{
}
#endif

