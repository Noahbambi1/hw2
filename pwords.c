#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <pthread.h>
#define NUM_CONSUMERS 4
#define MAXLINE 1000
#define MAXWORD 1024

typedef struct dict {
  char *word;
  int count;
  struct dict *next;
} dict_t;





// shared object
typedef struct sharedobject {
  FILE *rfile;  // file to read lines from
  char *buffer;  // word that will be passed from producer to consumer
  //char *line;   // next line to have read
  bool flag;     // to coordinate between a producer and consumers
  pthread_mutex_t flaglock;  // mutex for 'flag'
  pthread_cond_t flag_true;  // conditional variable for 'flag == true'
  pthread_cond_t flag_false;  // conditional variable for 'flag == false'
} so_t;

// arguments to consumer threads
// each thread needs to know it's number (for printing out)
// plus have access to the shared object
typedef struct targ {
  long tid;      // thread number
  so_t *soptr;   // pointer to shared object
  dict_t *dictionary;
} targ_t;



bool waittilltrue( so_t *so);
bool waittilfalse( so_t *so);
void *producer( void *arg );
void *consumer( void *arg );








char *
make_word( char *word ) {
  return strcpy( malloc( strlen( word )+1 ), word );
}

dict_t *
make_dict(char *word) {
  dict_t *nd = (dict_t *) malloc( sizeof(dict_t) );
  nd->word = make_word( word );
  nd->count = 1;
  nd->next = NULL;
  return nd;
}

dict_t *
insert_word( dict_t *d, char *word ) {
  
  //   Insert word into dict or increment count if already there
  //   return pointer to the updated dict
  
  dict_t *nd;
  dict_t *pd = NULL;		// prior to insertion point 
  dict_t *di = d;		// following insertion point
  // Search down list to find if present or point of insertion
  while(di && ( strcmp(word, di->word ) >= 0) ) { 
    if( strcmp( word, di->word ) == 0 ) { 
      di->count++;		// increment count 
      return d;			// return head 
    }
    pd = di;			// advance ptr pair
    di = di->next;
  }
  nd = make_dict(word);		// not found, make entry 
  nd->next = di;		// entry bigger than word or tail 
  if (pd) {
    pd->next = nd;
    return d;			// insert beond head 
  }
  return nd;
}

void print_dict(dict_t *d) {
  while (d) {
    printf("[%d] %s\n", d->count, d->word);
    d = d->next;
  }
}

int
get_word( char *buf, int n, FILE *infile) {
  int inword = 0;
  int c;  
  while( (c = fgetc(infile)) != EOF ) {
    if (inword && !isalpha(c)) {
      buf[inword] = '\0';	// terminate the word string
      return 1;
    } 
    if (isalpha(c)) {
      buf[inword++] = c;
    }
  }
  return 0;			// no more words
}




//dict_t *



int
main( int argc, char *argv[] ) {

 int status;
  pthread_t prod;                 // producer thread
  pthread_t cons[NUM_CONSUMERS];  // consumer threads
  targ_t carg[NUM_CONSUMERS];     
  void *ret;
  so_t *share = malloc( sizeof(so_t) );

  if( argc < 2 ) {
    fprintf( stderr, "Usage %s filename\n", argv[0] );
    exit( EXIT_FAILURE );
  }

  FILE * rfile = fopen( (char *) argv[1], "r" );
  if( !rfile ) {
    printf( "error opening %s\n", argv[0] );
    exit( EXIT_FAILURE );
  }

  share->rfile = rfile;
  //share->line = NULL;
  share->flag = false; // initially, the buffer is empty
  // initialize mutex; starts off unlocked
  if( (status = pthread_mutex_init( &share->flaglock, NULL )) != 0 )
    //err_abort( status, "mutex init" );
  // initialize conditional variables
  if( (status = pthread_cond_init( &share->flag_true, NULL )) != 0 )
    //err_abort( status, "flag_true init" );
  if( (status = pthread_cond_init( &share->flag_false, NULL )) != 0 )
    //err_abort( status, "flag_false init" );


  if( (status = pthread_create( &prod, NULL, producer, (void *) share )) != 0 )
    //err_abort( status, "create producer thread" );

  for( int i = 0; i < NUM_CONSUMERS; ++i ) {
    carg[i].tid = i;
    carg[i].soptr = share;
    if( (status =  pthread_create( &cons[i], NULL, consumer, &carg[i]) ) != 0 ){}
      //err_abort( status, "create consumer thread" );
  }

  printf("Producer and consumers created; main continuing\n");

  if( (status = pthread_join( prod, &ret )) != 0)
    //err_abort( status, "join producer thread" );
  printf( "main: producer joined with %d lines produced \n", *((int *) ret) );
  
  for (int i = 0; i < NUM_CONSUMERS; ++i) {
    if( (status = pthread_join( cons[i], &ret )) != 0)
    //err_abort( status, "join consumer thread" );
    printf( "main: consumer %d joined with %d lines consumed \n", i, *((int *) ret) );
  }

  if( (status = pthread_mutex_destroy( &share->flaglock )) != 0)
   // err_abort( status, "destroy mutex" );
  if( (status = pthread_cond_destroy( &share->flag_true )) != 0)
    //err_abort( status, "destroy flag_true" );
  if( (status = pthread_cond_destroy( &share->flag_false )) != 0)
   // err_abort( status, "destroy flag_false" );
  free( share );  // destroy shared object
  pthread_exit(NULL);
  
}




char *
readline( FILE *rfile ) {
  /* Read a line from a file and return a new line string object */
  char buf[MAXLINE];
  int len;
  char *result = NULL;
  char *line = fgets( buf, MAXLINE, rfile );
  if( line ) {
    len = strnlen( buf, MAXLINE );
    result = strncpy( malloc( len+1 ), buf , len+1 );
  }
  return result;
}

// are we waiting for the value of the flag to change to 'val'?
// argument 'tid' is for visualization only
bool
waittilltrue( so_t *so) {
  // wait until the codition "so->flag == true" is met
  int status;
  if( (status = pthread_mutex_lock( &so->flaglock  )) != 0 ) // lock the object to get access to the flag
   // err_abort( status, "lock mutex" );
  while( so->flag != true ) { // check the predicate associated with 'so->flag_true'
    printf( "waiting till 'true'\n" );
    // realease the lock and wait (done atomically)
    pthread_cond_wait( &so->flag_true, &so->flaglock ); // return locks the mutex
  }
  // we're holding the lock AND so->flag == val
  printf( " got 'true'\n" );
  return true;
}

// are we waiting for the value of the flag to change to 'val'?
// argument 'tid' is for visualization only
bool
waittillfalse( so_t *so) {
  // wait until the codition "so->flag == false" is met
  int status;
  if( (status = pthread_mutex_lock( &so->flaglock  )) != 0 ) // lock the object to get access to the flag
    //err_abort( status, "lock mutex" );
  while( so->flag == true ) { // check the predicate associated with 'so->flag_true'
    printf( " waiting till 'false'\n" );
    // realease the lock and wait (done atomically)
    pthread_cond_wait( &so->flag_false, &so->flaglock ); // return locks the mutex
  }
  // we're holding the lock AND so->flag == val
  printf( "got 'false'\n");
  return true;
}

int
releasetrue( so_t *so) {
  so->flag = true;
  printf( " set 'true'\n");
  pthread_cond_signal( &so->flag_true );
  return pthread_mutex_unlock( &so->flaglock );
}

int
releasefalse( so_t *so) {
  so->flag = false;
  printf( " set 'false'\n" );
  pthread_cond_signal( &so->flag_false );
  return pthread_mutex_unlock( &so->flaglock );
}

int
release_exit( so_t *so ) {
  pthread_cond_signal( &so->flag_true );
  return pthread_mutex_unlock( &so->flaglock );
}


void *
producer ( void *arg ) {
	#define PROD_ID 100
  int status;
  so_t *so = (so_t *) arg;
	int *ret = malloc( sizeof(int) ); // return value -- the number of lines produced
  int i = 0; // to count lines produced
	char wordbuf[MAXWORD];
	
	printf("Producer starting\n");


  while( get_word( wordbuf, MAXWORD, so->rfile ) ) {
    waittillfalse(so);
    so->buffer = wordbuf;		// put the line into the shared buffer
    i++;
    releasetrue( &so); // set flag to 'true', signal 'flag_true', and release the lock
    //wd = insert_word(wd, wordbuf); // add to dict
  }
  waittillfalse( so);
  so->buffer = NULL;
  releasetrue (&so);
  *ret = i;
  pthread_exit(ret);
}

void *
consumer (void *arg){
	int status;
  targ_t *targ = (targ_t *) arg;
  int *ret = malloc( sizeof(int) );  // return value -- the number of lines consumed
  int i = 0;
  so_t *so = targ->soptr;  // shared object
  char *wordbuf;
  dict_t *dictionary = targ->dictionary;
  
  while((wordbuf = so->buffer) && (waittilltrue(so))){
  	insert_word(dictionary,wordbuf);
  	releasefalse(so);
  	i++;
  }
  waittilltrue(so);
  so->buffer = NULL;
  releasefalse(&so);
  *ret = i;
  pthread_exit(ret);

}



