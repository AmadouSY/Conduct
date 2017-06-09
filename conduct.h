#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <complex.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

struct conduct{
  
  const char* nom;

  size_t taille_atomique;
  size_t capacite;
  size_t contenu;

  int eof;
  int closed;

  char* ecriture;
  char* lecture;

  char* debut;
  char* fin;

  pthread_mutex_t mutex;
  pthread_cond_t cond_lecture;
  pthread_cond_t cond_ecriture;

  //Flexible array member
  char tampon[] ;

};

struct iovec {
  int size;
  char *data[];
};


struct conduct *conduct_create(const char *name, size_t a, size_t c);
struct conduct *conduct_open(const char *name);
void conduct_close(struct conduct *conduct);
void conduct_destroy(struct conduct *conduct);
ssize_t conduct_read(struct conduct *c, void *buf, size_t count);
ssize_t conduct_write(struct conduct *c, const void *buf, size_t count);
int conduct_write_eof(struct conduct *c);
ssize_t conduct_writev(struct conduct *c, const struct iovec *iov, int iovcnt);
ssize_t conduct_readv(struct conduct *c, const struct iovec *iov, int iovcnt);
ssize_t conduct_readall(struct conduct *c, char* data[], int n);
ssize_t conduct_writeall(struct conduct *c, char* data[], int n);
