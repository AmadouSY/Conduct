#include "conduct.h"

/**
 * \file conduct.c
 * \brief Gestion de cs
 * \version 1.0
 * \date  26 avril 2017
 *
 */

/**
  * \param name Nom du c
  * \param a taille_atomique atomique
  * \param c Capacite
  * \return Un c
  * \author Amadou SY
  * \author Malick Gueye
  * \brief Un nouveau c de type conduct
  * \date 26 avril 2017
  * \version 1.0
*/
struct conduct*
conduct_create(const char* name, size_t a, size_t c)
{
  struct conduct *cond;
  size_t size = sizeof(struct conduct)+c;

  if(name == NULL)
  {
    //Conduit annonyme
    cond = mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (cond == MAP_FAILED){
      errno = EPIPE;
      return NULL;
    }
  }
  else
  {
    int fd;

    if(access(name, F_OK) != -1){
      errno = EEXIST;
      return NULL;
    }

    //Creation du fichier
    if((fd = open(name, O_CREAT | O_RDWR, 0666)) == -1)
    {
      close(fd);
      errno = EBADF;
      return NULL;
    }
    //Troncaturation (?) du fichier
    if (ftruncate(fd, size)  == -1)
    {
      close(fd);
      errno = EBADF;
      return NULL;
    }
    //Creation du Conduit
    cond = mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
    if (cond == MAP_FAILED) {
      errno = EPIPE;
      return NULL;
    }

    cond->nom = name;

  }

  //On initialise le mutex
  pthread_mutexattr_t mutex_attr;
  pthread_mutexattr_init(&mutex_attr);
  pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(&cond->mutex, &mutex_attr);

  //Puis les conditions
  pthread_condattr_t cond_attr;
  pthread_condattr_init(&cond_attr);
  pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
  pthread_cond_init(&cond->cond_lecture, &cond_attr);
  pthread_cond_init(&cond->cond_ecriture, &cond_attr);

  //Puis les autres variables
  //sprintf(cond->nom, name);
  cond->capacite = c;
  cond->taille_atomique= a;

  cond->eof = 0;
  cond->contenu = 0;
  cond->closed = 0;

  //Puis les pointeurs
  /*cond->debut = (void *) sizeof(struct conduct) + 1;
  cond->fin = sizeof(struct conduct) + c + 1;*/
  cond->debut= &cond->tampon[0];
  cond->fin=  &cond->tampon[c + 1];

  cond->ecriture= cond->debut;
  cond->lecture= cond->debut;


  return cond;

}

/**
  * \param name Nom du conduit
  * \return Le conduit
  * \author Amadou SY
  * \author Malick Gueye
  * \brief Ouvre le conduit corespondant
  * \date 26 avril 2017
  * \version 1.0
*/
struct conduct
*conduct_open(const char *name)
{

  struct conduct *cond;
  struct stat st;
  int fd;

  //On ouvre le fichier
  if((fd = open(name, O_RDWR,0666)) == -1){
    close(fd);
    errno = ENOENT;
    return NULL;
  }
  //On recupere la taille
  if(fstat (fd, &st) == -1) {
    errno = ENOENT;
    return NULL;
  }
  //Creation du conduit
  cond = (struct conduct *) mmap(NULL, st.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
  if (cond == MAP_FAILED){
    errno = EPIPE;
    return NULL;
  }

  close(fd);
  cond->closed = 0;

  return cond ;

}

/**
  * \param c Un conduit
  * \author Amadou SY
  * \author Malick Gueye
  * \brief Ferme le conduit
  * \date 26 avril 2017
  * \version 1.0
*/
void
conduct_close(struct conduct * c)
{
  msync(c, sizeof(c), MS_SYNC);
  munmap(c, sizeof(c));
}

/**
  * \param c Un conduit
  * \author Amadou SY
  * \author Malick Gueye
  * \brief Detruit le conduit
  * \date 26 avril 2017
  * \version 1.0
*/
void
conduct_destroy(struct conduct * c)
{
  msync(c, sizeof(c), MS_SYNC);
  munmap(c, sizeof(c));
  if(c->nom != NULL)
    remove(c->nom);
}

/**
  * \param c Le conduit a lire
  * \param buf L emplacement de lecture
  * \param count la quantite a lire
  * \return La quantite lue
  * \author Amadou SY
  * \author Malick Gueye
  * \brief Lit une portion de conduit donnee
  * \date 1 mai 2017
  * \version 2.0
*/
ssize_t
conduct_read(struct conduct *c, void *buf, size_t count)
{

  pthread_mutex_lock(&(c->mutex));

  //Conduit vide
  if(c->contenu == 0)
  {
    //Avec marque de fin de fichier
    if (c->eof == 1)
    {
      pthread_mutex_unlock(&c->mutex);
      return 0;
    }
    else
    {
      //Attendre une ecriture ou insertion d'un eof
      while((c->contenu == 0) && (c->eof == 0)){
        pthread_cond_wait(&(c->cond_ecriture), &(c-> mutex));

      }
    }
  }

  if((c->contenu == 0) && c->eof) {
    pthread_cond_broadcast(&c->cond_ecriture);
    pthread_mutex_unlock(&c->mutex);
    return 0;
  }

  if(count > c->contenu) count = c->contenu;


  size_t part = 0;

  if(c->ecriture < c->lecture){
    part = (c->fin - c->lecture);

    if(part > count) part = count;

    memcpy(buf, c->lecture, part);
    (c->lecture+part >= c->fin) ?
      c->lecture = c->debut : (c->lecture += part);
    c->contenu -= part;

  }
  if(count-part > 0){
    memcpy(buf+part, c->lecture, count-part);
    c->lecture += count-part;
    c->contenu -= count-part;
  }

  pthread_cond_broadcast(&c->cond_ecriture);
  pthread_mutex_unlock(&c->mutex);

  return count;
}


/**
  * \param c Le conduit
  * \param buf
  * \param count
  * \return La quantite ecrit
  * \author Amadou SY
  * \author Malick Gueye
  * \brief Lit une portion de conduit donnee
  * \date 1 mai 2017
  * \version 2.0
*/
ssize_t
conduct_write(struct conduct* c, const void* buf, size_t count)
{
  pthread_mutex_lock(&c->mutex);
  if(c->eof == 1)
  {
    pthread_mutex_unlock(&c->mutex);
    errno = EPIPE;
    return -1;
  }

  while((count <= c->taille_atomique
      && (c->capacite - c->contenu) < count)
      || (c->capacite - c->contenu) == 0)
  {
    pthread_cond_wait(&c->cond_lecture, &c->mutex);
    if(c->eof){
      pthread_cond_broadcast(&c->cond_lecture);
      pthread_mutex_unlock(&(c->mutex));
      errno = EPIPE;
      return -1;
    }
  }

  if(count > c->capacite - c->contenu) count = c->capacite - c->contenu;

  size_t part_count = 0;
  while (count <= c->taille_atomique && part_count < count)
  {
    size_t part;

    part = c->ecriture >= c->lecture ?
      c->fin - c->ecriture : (c->lecture-1) - c->ecriture;

    if(part>count-part_count){
      part = count-part_count;
    }

    memcpy(c->ecriture, buf+part_count, part);

    (c->ecriture+part >= c->fin) ? (c->ecriture = c->debut) : (c->ecriture += part);

    part_count+= part;
  }

  c->contenu += count;

  pthread_cond_broadcast(&c->cond_ecriture);
  pthread_mutex_unlock(&c->mutex);
  return count;

}


/**
  * \param c Le conduit a bloquer
  * \return 1 si reussi
  * \author Amadou SY
  * \author Malick Gueye
  * \brief Insert une marque de fin de fichier
  * \date 1 mai 2017
  * \version 2.0
*/
int
conduct_write_eof(struct conduct *c)
{
  //pthread_mutex_lock(&(c->mutex));

  //On insere la marque si necessaire
  if(c->eof != 1)
    c->eof = 1;

  //Si on une marque est deja presente
  //Attaque trempette

  //On debloque le mutex, reveil la cond puis retourne 1
  //pthread_mutex_unlock(&(c->mutex));
  //pthread_cond_signal (&c->condition);

  return 1;

}


/**
  * \param c Le conduit a lire
  * \param iov Suite de tampon
  * \param iovcnt la taille de iov
  * \return La quantite a lire
  * \author Amadou SY
  * \author Malick Gueye
  * \brief Concatene puis lit des donnees
  * \date 2 juin 2017
  * \version 2.0
*/
ssize_t
conduct_readv(struct conduct *c, const struct iovec *iov, int iovcnt)
{
  int i;
  char * bd = iov->data[0];
  ssize_t size = 0;

  for(i = 1; i < iovcnt; i++){
    strcat(bd, iov->data[i]);
    size += sizeof(iov->data[i]);
  }
  int read;
  if((read = conduct_read(c, (void *) bd, size)) == -1){
    errno = EPIPE;
    return -1;
  }
  return read;
}

/**
  * \param c Le conduit sur lequel ecrire
  * \param iov Suite de tampon
  * \param iovcnt la taille de iov
  * \return La quantite a ecrire
  * \author Amadou SY
  * \author Malick Gueye
  * \brief Concatene puis ecrit des donnees
  * \date 2 juin 2017
  * \version 2.0
*/
ssize_t
conduct_writev(struct conduct *c, const struct iovec *iov, int iovcnt)
{
  int i;
  char * bd = iov->data[0];
  ssize_t size = 0;
  for(i = 1; i < iovcnt; i++){
    strcat(bd, iov->data[i]);
    size += sizeof(iov->data[i]);
  }
  int write;
  if((write = conduct_write(c, (void *) bd, size)) == -1){
    errno = EPIPE;
    return -1;
  }
  return write;
}

/**
  * \param c Le conduit a lire
  * \param data Suite de tampon
  * \param n la taille de data
  * \return La quantite lu
  * \author Amadou SY
  * \author Malick Gueye
  * \brief Fais n lecture
  * \date 2 juin 2017
  * \version 2.0
*/
ssize_t
conduct_readall(struct conduct *c, char* data[], int n)
{
  int i;
  int read = 0;
  for(i = 0; i < n; i++){
    if(read += conduct_read(c, (void *) data[i], sizeof(data[i])) == -1){
      break;
    }
  }
  return read;
}


/**
  * \param c Le conduit a lire
  * \param data Suite de tampon
  * \param n la taille de data
  * \return La quantite ecrit
  * \author Amadou SY
  * \author Malick Gueye
  * \brief Fais n ecriture
  * \date 2 juin 2017
  * \version 2.0
*/
ssize_t
conduct_writeall(struct conduct *c, char* data[], int n)
{
  int i;
  int write = 0;
  for(i = 0; i < n; i++){
    if(write += conduct_write(c, (void *) data[i], sizeof(data[i])) == -1){
      break;
    }
  }
  return write;
}
