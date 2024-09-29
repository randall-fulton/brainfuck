#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  INST_HALT, // ensures zero-initialization always halts
  INST_DP_INC,
  INST_DP_DEC,
  INST_BYTE_INC,
  INST_BYTE_DEC,
  INST_OUTPUT,
  INST_INPUT,
  INST_JMP_FWD,
  INST_JMP_BACK,
} InstType;

typedef struct {
  InstType type;
  size_t jump_ip;
} Inst;

#define DATA_STACK_SIZE 8
#define INST_STACK_SIZE 256

typedef struct {
  Inst instructions[INST_STACK_SIZE];
  char data[DATA_STACK_SIZE];
  
  size_t ip;
  size_t dp;
} Machine;

#define JUMP_STACK_MAX 16

void machine_parse(Machine *m, const char *source) {
  size_t jump_stack[JUMP_STACK_MAX] = {0};
  size_t jump_sp = 0;

  bool in_comment = false;
  size_t pos = 0;
  size_t ip = 0;
  while (source[pos] != 0) {
    if (in_comment && source[pos] != '\n') {
      pos++;
      continue;
    }
    
    switch (source[pos]) {
    case '>': { m->instructions[ip++] = (Inst){ .type = INST_DP_INC }; }; break;
    case '<': { m->instructions[ip++] = (Inst){ .type = INST_DP_DEC }; }; break;
    case '+': { m->instructions[ip++] = (Inst){ .type = INST_BYTE_INC }; }; break;
    case '-': { m->instructions[ip++] = (Inst){ .type = INST_BYTE_DEC }; }; break;
    case '.': { m->instructions[ip++] = (Inst){ .type = INST_OUTPUT }; }; break;
    case ',': { m->instructions[ip++] = (Inst){ .type = INST_INPUT }; }; break;
    case '[': {
      assert(jump_sp <= JUMP_STACK_MAX);
      jump_stack[jump_sp++] = ip;
      m->instructions[ip++] = (Inst){
      	.type = INST_JMP_FWD,
      	.jump_ip = 0,
      };
    }; break;
    case ']': {
      // back-patch start of jump with end ip
      assert(jump_sp > 0);
      size_t jump_start_ip = jump_stack[jump_sp-1];
      Inst *jump_start = &m->instructions[jump_start_ip];
      assert(jump_start->type == INST_JMP_FWD);
      jump_start->jump_ip = ip;
      
      m->instructions[ip++] = (Inst){
      	.type = INST_JMP_BACK,
	.jump_ip = jump_start_ip,
      };
      jump_sp--;
    }; break;
    case ' ':
    case '\t': break;
    case '\n': in_comment = false; break;
    case 0: return;
    default: in_comment = true; break;
    }
    pos++;
  }
}

void machine_dump(Machine *m) {
  printf("[");
  for (int i = 0; i < DATA_STACK_SIZE; i++) {
    if (i > 0) {
      printf(" %2d", m->data[i]);
    } else {
      printf("%2d", m->data[i]);
    }
  }
  printf("]\n");
  char pointer[3*DATA_STACK_SIZE+2] = "                          ";
  pointer[m->dp * 3 + 2] = '^';
  printf("%s\n", pointer);
}

bool machine_run_next(Machine *m) {
  switch (m->instructions[m->ip].type) {
  case INST_HALT: return false; break;
  case INST_DP_INC: m->dp++; break;
  case INST_DP_DEC: m->dp--; break;
  case INST_BYTE_INC: m->data[m->dp]++; break;
  case INST_BYTE_DEC: m->data[m->dp]--;  break;
  case INST_OUTPUT: printf("%c", m->data[m->dp]); break;
  case INST_INPUT: m->data[m->dp] = getchar(); break;
  // branchless jumps, because why not?
  case INST_JMP_FWD: {
    m->ip =
      m->ip * (m->data[m->dp] != 0) +
      m->instructions[m->ip].jump_ip * (m->data[m->dp] == 0);
  } break;
  case INST_JMP_BACK: {
    m->ip =
      m->ip * (m->data[m->dp] == 0) +
      m->instructions[m->ip].jump_ip * (m->data[m->dp] != 0);
  } break;
  }
  
  m->ip++;
  return m->instructions[m->ip].type != INST_HALT;
}

const char *shift(int *argc, char ***argv) {
  assert(argc > 0);
  const char *arg = (*argv)[0];
  *argc -= 1;
  *argv += 1;
  return arg;
}

char *read_full_file(const char *file_path) {
  FILE *f = fopen(file_path, "rb");
  assert(f && "could not open file");

  fseek(f, 0, SEEK_END);
  int size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *contents = malloc(sizeof(char)*size);
  assert(contents && "not enough memory");
  fread(contents, sizeof(char), size, f);

  fclose(f);
  return contents;
}

void usage(const char *program) {
  printf("Usage: %s <file>\n", program);
}

int main(int argc, char **argv) {
  const char *program = shift(&argc, &argv);
  if (argc <= 0) {
    usage(program);
    fprintf(stderr, "ERR: expected file\n");
    return 1;
  }
  
  const char *file_path = shift(&argc, &argv);
  char *source = read_full_file(file_path);
  if (source == NULL) {
    fprintf(stderr, "ERR: could not read source file, %s", strerror(errno));
    return 1;
  }

  Machine m = {0};
  machine_parse(&m, source);

  bool run_again = true;
  do {
    run_again = machine_run_next(&m);
    /* machine_dump(&m); */
  } while (run_again);
  
  free(source);
  return 0;
}
