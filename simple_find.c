#define _BSD_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <strings.h>
#include <features.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "limit_fork.h"
#define NONDIR_TYPE 0
#define DIR_TYPE 1
#define CHKTYPE_ERROR -1
#define DEFAULT_DICT_SIZE 3
#define DEFAULT_BUF_SIZE 100
#define DEFAULT_TABLE_SIZE 10

struct table;

struct entry
{
   int type;
   char *filename;
};

struct table
{
   int size;
   struct entry **table_entry;
};

void *checked_malloc(size_t size)
{
   void *p = malloc(size);
   if (!p) {
      perror("");
      exit(-1);
   }
   return p;
}

void *checked_realloc(void *p, size_t size)
{
   void *q = realloc(p, size);
   if (!q) {
      perror("");
      exit(-1);
   }
   return q;
}

int check_type(char *pathname)
{
   struct stat st;
   if (lstat(pathname, &st)) {
      perror(pathname);
      return CHKTYPE_ERROR;
   }
   if (S_ISDIR(st.st_mode))
      return DIR_TYPE;
   return NONDIR_TYPE;
}

struct entry *process_file(char *filename, char *key, int is_first)
{
   int type;
   struct entry *ety;
   if (!is_first && (strcmp(filename, ".")==0 || strcmp(filename, "..")==0))
      return NULL;
   type = check_type(filename);
   if (is_first && (type==CHKTYPE_ERROR))
      return NULL;
   if (key && !strstr(filename, key) && type != DIR_TYPE)
      return NULL;
   ety = checked_malloc(sizeof(struct entry));
   ety->filename = checked_malloc(strlen(filename)+1);
   strcpy(ety->filename, filename);
   ety->type = (type==DIR_TYPE) ? DIR_TYPE : NONDIR_TYPE;
   return ety;
}

int cmp(const void *p, const void *q)
{
   struct entry *e1;
   struct entry *e2;
   if (p==NULL && q==NULL) return 0;
   if (p == NULL) return -1;
   if (q == NULL) return 1;
   e1 = *(struct entry **)p;
   e2 = *(struct entry **)q;
   return strcasecmp(e1->filename, e2->filename);
}

struct table *new_table(int size, struct entry **table_entry)
{
   struct table *t = checked_malloc(sizeof(struct table));
   t->size = size;
   t->table_entry = table_entry;
   return t;
}

struct table *build_table(char *pathname, char *key, int is_first)
{
   DIR *dp;
   int size = 0;
   int tb_size = DEFAULT_TABLE_SIZE;
   struct dirent *f;
   struct entry **table_entry = NULL;
   if (is_first) {
      table_entry = checked_malloc(sizeof(struct entry *));
      table_entry[0] = process_file(pathname, key, is_first);
      if (table_entry[0] == NULL) {
         free(table_entry);
         return NULL;
      }
      return new_table(1, table_entry);
   }
   if (!(dp = opendir(pathname))) {
      perror(pathname);
      return NULL;
   }
   if (chdir(pathname)) {
      perror(pathname);
      closedir(dp);
      return NULL;
   }
   while ((f = readdir(dp))) {
      struct entry *ety;
      if ((ety = process_file(f->d_name, key, is_first)) == NULL)
         continue;
      if (!table_entry)
         table_entry = checked_malloc(tb_size * sizeof(struct entry *));
      if (size+1 > tb_size) {
         tb_size *= 2;
         table_entry = checked_realloc(table_entry, tb_size * sizeof(struct entry *));
      }
      table_entry[size++] = ety;
   }
   closedir(dp);
   qsort(table_entry, size, sizeof(table_entry[0]), cmp);
   return new_table(size, table_entry);
}

void free_entry(struct entry *e)
{
   free(e->filename);
   free(e);
}

void free_table(struct table *t)
{
   struct entry **table_entry = t->table_entry;
   free(table_entry);
   free(t);
}

int generate_path(char *filename, char **buffer, int p_offset, int *buf_size)
{
   int off_set = p_offset;
   char *buf_start;
   char *buf = *buffer;
   if (!buf) {
      *buf_size = DEFAULT_BUF_SIZE;
      buf = checked_malloc(*buf_size);
      buf_start = buf;
      buf[0] = '\0';
      strcpy(buf, filename);
      off_set = strlen(filename);
   } else {
      if (strlen(filename)+off_set+1 >= *buf_size) {
         *buf_size += 2 * strlen(filename);
         buf = checked_realloc(buf, *buf_size);
      }
      buf_start = buf;
      buf += off_set;
      if (*(buf-1) != '/') {
         *buf = '/';
         buf++;
         off_set++;
      }
      strcpy(buf, filename);
      off_set += strlen(filename);
   }
   *buffer = buf_start;
   return off_set;
}

void execute(char *path, char *prog_argv[], int *substitute_dict, int initial_fd)
{
   int i;
   pid_t pid;
   if (prog_argv[0] == NULL) return;
   for (i=0; substitute_dict[i]!=-1; i++)
      prog_argv[substitute_dict[i]] = path;
   if ((pid = fork()) < 0) {
      perror("");
      return;
   } else if (pid == 0) {
      fchdir(initial_fd);
      if (execvp(prog_argv[0], prog_argv) < 0) {
         perror(prog_argv[0]);
         exit(-1);
      }
   } else {
      wait(NULL);
   }
}

void traverse(char *pathname, char *key, int is_first, 
              char **buffer, int p_offset, int *buf_size,
              int initial_fd, char **prog_argv, int *substitute_dict)
{
   int i;
   struct table *t;
   struct entry **table_entry;
   if ((t = build_table(pathname, key, is_first)) == NULL)
      return;
   table_entry = t->table_entry;
   for (i=0; i<t->size; i++) {
      int tmp = generate_path(table_entry[i]->filename, buffer, p_offset, buf_size);
      if (table_entry[i]->type == NONDIR_TYPE) {
         if (prog_argv == NULL)
            printf("%s\n", *buffer);
         else execute(*buffer, prog_argv, substitute_dict, initial_fd);
      } else if (table_entry[i]->type == DIR_TYPE) {
         if (key==NULL || (key!=NULL && strstr(table_entry[i]->filename, key))) {
            if (prog_argv == NULL)
               printf("%s\n", *buffer);
            else execute(*buffer, prog_argv, substitute_dict, initial_fd);
         }
         traverse(table_entry[i]->filename, key, 0, buffer, tmp, buf_size,
                  initial_fd, prog_argv, substitute_dict);
      }
      free_entry(table_entry[i]);
   }
   free_table(t);
   chdir("..");
}

void arg_err()
{
   fprintf(stderr, "simple_find: invalid arguments \n");
   exit(-1);
}

char **parse_argv(int argc, char *argv[], char **filename, char **key)
{
   int i;
   int print = 0;
   int arg_end = 0;
   char *_key = NULL;
   char *_filename = NULL;
   char **prog_argv = NULL;
   for (i=1; i<argc; i++) {
      if (arg_end) arg_err();
      if (argv[i][0] != '-') {
         if (_filename) arg_err();
         _filename = argv[i];
      } else if (strcmp(argv[i], "-name") == 0) {
         if (_key) arg_err();
         if (i+1 >= argc) arg_err();
         _key = argv[++i];
      } else if (strcmp(argv[i], "-exec") == 0) {
         if (prog_argv) arg_err();
         if (i+1 >= argc) arg_err();
         prog_argv = &argv[i+1];
         while (argv[++i][0] != ';') {
            if (i+1 >= argc) {
               fprintf(stderr, "simple_find: no end symbol \\; for exec cmd\n");
               exit(-1);
            }
         }
         arg_end = 1;
         argv[i] = NULL;
      } else if (strcmp(argv[i], "-print") == 0)
         print = 1;
      else arg_err();
   }
   if (print==1 && prog_argv) arg_err();
   if (!_filename) _filename = ".";
   if (!print && !prog_argv) exit(0);
   *key = _key;
   *filename = _filename;
   return prog_argv;
}

int *generate_dict(char **prog_argv)
{
   int i;
   int *dict = NULL;
   int size = 1;
   int dict_size = DEFAULT_DICT_SIZE;
   if (!prog_argv) return NULL;
   dict = checked_malloc(dict_size * sizeof(int));
   for (i=0; prog_argv[i]!=NULL; i++) {
      if (strcmp(prog_argv[i], "{}") == 0) {
         if (size+1 > dict_size) {
            dict_size *= 2;
            dict = checked_realloc(dict, dict_size * sizeof(int));
         }
         dict[(size++)-1] = i;
      }
   }
   dict[size-1] = -1;
   return dict;
}

int main(int argc, char *argv[])
{
   int initial_fd;
   int buf_size = 0;
   int *dict;
   char *key;
   char *filename;
   char **prog_argv;
   char *buffer = NULL;
   limit_fork(10);
   initial_fd = open(".", O_RDONLY);
   prog_argv = parse_argv(argc, argv, &filename, &key);
   dict = generate_dict(prog_argv);
   traverse(filename, key, 1, &buffer, 0, &buf_size, initial_fd, prog_argv, dict);
   if (buffer) free(buffer);
   if (dict) free(dict);
   close(initial_fd);
   return 0;
}
