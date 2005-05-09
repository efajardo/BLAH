#include "BLParserLSF.h"

int main(int argc, char *argv[]) {

    short int port;                  /*  port number               */
    struct    sockaddr_in servaddr;  /*  socket address structure  */
    char      *endptr;                /*  for strtol()              */
    int       i;
    int       status;
    int       list_s;

    char eventsfile[MAX_CHARS]="\0";

    pthread_t ReadThd[NUMTHRDS];
    pthread_t UpdateThd;

    argv0 = argv[0];

    /*  Get port number from the command line, and
        set to default port if no arguments were supplied  */

    if ( argc == 2 ) {
	port = strtol(argv[1], &endptr, 0);
	if ( *endptr ) {
	    fprintf(stderr, "BLParserLSF: Invalid port number.\n");
	    exit(EXIT_FAILURE);
	}
    }
    else if ( argc < 2 ) {
	port = ECHO_PORT;
    }
    else {
	fprintf(stderr, "BLParserLSF: Invalid arguments.\n");
	exit(EXIT_FAILURE);
    }
    
    /* Get log dir name */

   ldir=GetLogDir();
    /* For local tests */
   //ldir="/home/mezzadri/src/logserver/final/logs";
    
    strcat(eventsfile,ldir);
    strcat(eventsfile,"/");
    strcat(eventsfile,lsbevents);
    
    /* Get all events from lsb.events */
    
     GetAllEvents(eventsfile);

    /*  Create the listening socket  */

    if ( (list_s = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
	fprintf(stderr, "BLParserLSF: Error creating listening socket.\n");
	exit(EXIT_FAILURE);
    }


    /*  Set all bytes in socket address structure to
        zero, and fill in the relevant data members   */

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(port);


    /*  Bind our socket addresss to the 
	listening socket, and call listen()  */

    if ( bind(list_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ) {
	fprintf(stderr, "BLParserLSF: Error calling bind()\n");
	exit(EXIT_FAILURE);
    }
    
    if ( listen(list_s, LISTENQ) < 0 ) {
    	fprintf(stderr, "BLParserLSF: Error calling listen()\n");
    	exit(EXIT_FAILURE);
    }
   
    for(i=0;i<NUMTHRDS;i++){
     pthread_create(&ReadThd[i], NULL, LookupAndSend, (void *)list_s);
    }

    pthread_create(&UpdateThd, NULL, mytail, (void *)eventsfile);
    pthread_join(UpdateThd, (void **)&status);
    
   pthread_exit(NULL);
 
}

/*---functions---*/

/*  Read a line from a socket  */

ssize_t Readline(int sockd, void *vptr, size_t maxlen) {
    ssize_t n, rc;
    char    c, *buffer;

    buffer = vptr;

    for ( n = 1; n < maxlen; n++ ) {
	
	if ( (rc = read(sockd, &c, 1)) == 1 ) {
	    *buffer++ = c;
	    if ( c == '\n' )
		break;
	}
	else if ( rc == 0 ) {
	    if ( n == 1 )
		return 0;
	    else
		break;
	}
	else {
	    if ( errno == EINTR )
		continue;
	    return -1;
	}
    }

    *buffer = 0;
    return n;
}

/*  Write a line to a socket  */

ssize_t Writeline(int sockd, const void *vptr, size_t n) {
    size_t      nleft;
    ssize_t     nwritten;
    const char *buffer;
    

    buffer = vptr;
    nleft  = n;

     while ( nleft > 0 ) {
 	if ( (nwritten = write(sockd, (char *)vptr, nleft)) <= 0 ) {
	    if ( errno == EINTR ) {
		nwritten = 0;
	    }else{
		return -1;
	    }
	}
	nleft  -= nwritten;
	buffer += nwritten;
    }

    return n;
}

unsigned hash(char *s){

 unsigned hashval;

 for(hashval = 0; *s!='\0';s++){
  hashval = *s + 31 *hashval;
 }
 return hashval % HASHSIZE;
}


void *mytail (void *infile){    
        
    char **lines;
    int i, nlines = NLINES;
    
    nlines++;
    if((lines=malloc(nlines * sizeof *lines)) == 0){
        sysfatal("can't malloc lines: %r");
    }
    for(i=0; i < nlines; i++){
        if((lines[i]=malloc(MAX_CHARS)) == 0){
            sysfatal("can't malloc lines[%d]: %r", i);
	}
    }
 
   follow((char *)infile, lines, nlines);
   
    for(i=0; i < nlines; i++){
     free(lines[i]);
    }
    free(lines);

   return 0;
}

void
follow(char *infile, char *lines[], int n)
{
    FILE *fp;
    long off = 0;
    long old_off = 0;
    long real_off = 0;

    for(;;){
        if((fp=fopen((char *)infile, "r")) == 0){
         syserror("error opening %s: %r", infile);
        }
        if(fseek(fp, off, SEEK_SET) < 0){
         sysfatal("couldn't seek: %r");
        }

        old_off=ftell(fp);
        fseek(fp, 0L, SEEK_END);
        real_off=ftell(fp);

        if(real_off < old_off){
         off=real_off;
        }else{
         off=old_off;
        }
   
        if(fseek(fp, off, SEEK_SET) < 0){
          sysfatal("couldn't seek: %r");
         }
        
        off = tail(fp, lines, n);
	fclose(fp);
	sleep(1);
    }        
}

long
tail(FILE *fp, char *lines[], int n)
{
    int i, j;
    long off=0;

    for(i=0; i < n; i++){
        *lines[i] = NUL;
    }
    i=0;

    while(fgets(lines[i], MAX_CHARS, fp)){
      if((strstr(lines[i],rex_queued)!=NULL) || (strstr(lines[i],rex_running)!=NULL) || (strstr(lines[i],rex_status)!=NULL) || (strstr(lines[i],rex_signal)!=NULL)){
        if(++i == n){
            i = 0;
	}
      }else{
       *lines[i] = NUL;
      }
    }

    j = i;
    i = (i+1 == n ? 0 : i+1);
    while(i != j){
     if(*lines[i] != NUL){
       AddToStruct(lines[i]);
     }

     if(++i == n){
     	i = 0;
     }
    }
    if((off=ftell(fp)) < 0){
        sysfatal("couldn't get file location: %r");
    }
    return off;
}


int InfoAdd(int id, char *value, const char * flag){

/*
 time_t now;
 struct tm *tptr;
 char cnow[30];
 

 now=time(NULL); 
 tptr=localtime(&now);
 strftime(cnow,sizeof(cnow),"%s",tptr);
*/

 if((id <= 0) || (id >= HASHSIZE)){
  return -1;
 }
  
 /* set write lock */
  wlock=1;
 if((strcmp(flag,"JOBID")==0) && j2js[id] == NULL){
 
  j2js[id] = strdup("1");
  j2wn[id] = strdup("\0");
  j2ec[id] = strdup("\0");
  
 } else if(strcmp(flag,"WN")==0){
 
  j2wn[id] = strdup(value);
  
 } else if(strcmp(flag,"JOBSTATUS")==0){
 
  j2js[id] = value;
 
 } else if(strcmp(flag,"EXITCODE")==0){

  j2ec[id] = strdup(value);
 
 } else {
 
 /* release write lock */
    wlock=0;
    
   return -1;
 
 }
   /* release write lock */
    wlock=0;
    
   return 0;
}

int AddToStruct(char *line){

 int n=0;
 int has_blah=0;
 unsigned h_blahjob;
 char *s_tok;
 char *	rex;
 
 int id;
 
 char *	jobid=NULL;
 char *	j_status=NULL;
 char *	ex_status=NULL;
 char *	sig_status=NULL;
 char *	j_blahjob=NULL;
 char *	wnode=NULL;
 char *blahjob_string="blahjob_";

 if(strstr(line,blahjob_string)!=NULL){
  has_blah=1;
 }
 s_tok=strtok(line,blank);

 while(s_tok!=NULL){
  if(n==0){
   rex=strdup(s_tok);
  }else if(n==3){
   jobid=strdup(s_tok);
   id=atoi(jobid);
  }else if(n==4){
   j_status=strdup(s_tok);
  }else if(n==6){
   sig_status=strdup(s_tok);
  }else if(n==9){
   wnode=strdup(s_tok);
  }else if(n==10){
   ex_status=strdup(s_tok);
  }else if(n==41){
   j_blahjob=strdup(s_tok);
  }
  s_tok=strtok(NULL,blank);
  n++;
 }

 while (1){
  if(rcounter==0){
   break;
  }
  sleep(1);
 } 
 
 if((strcmp(rex,rex_queued)==0) && (has_blah) && (j2js[id]==NULL)){

  InfoAdd(id,jobid,"JOBID");
 
  h_blahjob=hash(j_blahjob);
  bjl[h_blahjob]=strdup(jobid);
  
 } else if(j2js[id]!=NULL){
 

  if(strcmp(rex,rex_running)==0){

   InfoAdd(id,"2","JOBSTATUS");
   InfoAdd(id,wnode,"WN");
   
  } else if(strcmp(rex,rex_signal)==0){
  
   if(strstr(sig_status,"KILL")!=NULL){

    InfoAdd(id,"3","JOBSTATUS");

   }
     
  } else if(strcmp(rex,rex_status)==0){
  
   if(strstr(j_status,"192")!=NULL){

    InfoAdd(id,"4","JOBSTATUS");
    InfoAdd(id,"0","EXITCODE");

   }  else if(strstr(j_status,"32")!=NULL){

    if(strcmp(j2js[id],"3")!=0){
     InfoAdd(id,"4","JOBSTATUS");
     InfoAdd(id,ex_status,"EXITCODE");
    }

   } else if(strstr(j_status,"16")!=NULL){

    InfoAdd(id,"5","JOBSTATUS");

   } else if(strstr(j_status,"4")!=NULL){

    InfoAdd(id,"2","JOBSTATUS");

   }
  } /* closes if-else if on rex_ */
 } /* closes if-else if on jobid lookup */
   free(rex);
   free(jobid);
   free(j_status);
   free(sig_status);
   free(wnode);
   free(ex_status);
   free(j_blahjob);

 return 0;
}

char *GetAllEvents(char *file){
 
 FILE *fp;
 char *line;
 char *opfile[STR_CHARS];
 int i=0;
 int maxtok;

 maxtok = strtoken(file, ' ', opfile);

 for(i=0; i<maxtok; i++){ 
  if((line=malloc(MAX_LINES)) == 0){
   sysfatal("can't malloc line: %r");
  }
 
  if((fp=fopen(opfile[i], "r")) != 0){
   while(fgets(line, MAX_LINES, fp)){
    if((strstr(line,rex_queued)!=NULL) || (strstr(line,rex_running)!=NULL) || (strstr(line,rex_status)!=NULL) || (strstr(line,rex_signal)!=NULL)){
     AddToStruct(line);
    }
   }
  } else {
   printf("Cannot open %s file\n",opfile[i]);
   exit(-1);
  }
  fclose(fp);
  free(line);


 } /* close for*/
    
 return NULL;

}

void *LookupAndSend(int m_sock){ 
    
    char      *buffer;
    char      *out_buf;
    char      *logdate;
    char      *jobid;
    char      h_jobid[20];
    char      t_wnode[STR_CHARS];
    char      *pr_removal="Not";
    int       i;
    int       id;
    int       conn_s;
    
    while ( 1 ) {


	
	/*  Wait for a connection, then accept() it  */
	
	if ( (conn_s = accept(m_sock, NULL, NULL) ) < 0 ) {
	    fprintf(stderr, "BLParserLSF: Error calling accept()\n");
	    exit(EXIT_FAILURE);
	}

	if((buffer=malloc(STR_CHARS)) == 0){
	  sysfatal("can't malloc buffer in LookupAndSend: %r");
	}
       buffer[0]='\0';

        /* read line from socket */
	Readline(conn_s, buffer, STR_CHARS-1);
	
	//printf("thread/0x%08lx\n",pthread_self());
	
	if((strlen(buffer)==0) || (strcmp(buffer,"\n")==0) || (strstr(buffer,"/")==0)){
         if((out_buf=malloc(STR_CHARS)) == 0){
          sysfatal("can't malloc out_buf in LookupAndSend: %r");
         }
     	 sprintf(out_buf,"Wrong string format/Not\n");
	 goto close;
	}
        logdate=strtok(buffer,"/");
        jobid=strtok(NULL,"/");
        strtok(jobid,"\n");
        
/* get jobid from blahjob id (needed by lsf_submit.sh) */
       
	if(strcmp(logdate,"BLAHJOB")==0){
         for(i=0;i<WRETRIES;i++){
	  if(wlock==0){
	   *h_jobid=NUL;
	   strcat(h_jobid,"\"");
	   strcat(h_jobid,jobid);
	   strcat(h_jobid,"\"");
	   if(bjl[hash(h_jobid)]==NULL){
	    sleep(1);
	    continue;
	   }
           if((out_buf=malloc(STR_CHARS)) == 0){
            sysfatal("can't malloc out_buf in LookupAndSend: %r");
           }
     	   sprintf(out_buf,"%s/Not\n",bjl[hash(h_jobid)]);
	   goto close;
	  }else{
	   sleep(1);
	  } 
	 }
	 if(i==WRETRIES){
          if((out_buf=malloc(STR_CHARS)) == 0){
           sysfatal("can't malloc out_buf in LookupAndSend: %r");
          }
	  sprintf(out_buf,"Blahjob id %s not found/Not\n",h_jobid);
	  goto close;
	 }
	}
	
	
/* get all info from jobid */
     
        for(i=0;i<WRETRIES;i++){
	
	 if(wlock==0){
	 
 	  id=atoi(jobid);
	  
    	  if(j2js[id]!=NULL){
 
           if((out_buf=malloc(STR_CHARS)) == 0){
            sysfatal("can't malloc out_buf in LookupAndSend: %r");
           }
	   	   
           if(strcmp(j2wn[id],"\0")==0){
            t_wnode[0]='\0';
           }else{
            sprintf(t_wnode,"WorkerNode=%s;",j2wn[id]);
           }
           if((strcmp(j2js[id],"3")==0) || (strcmp(j2js[id],"4")==0)){
            pr_removal="Yes";
           } else {
            pr_removal="Not";
           }
           if(strcmp(j2js[id],"4")==0){
            sprintf(out_buf,"[BatchJobId=\"%s\"; %s JobStatus=%s; ExitCode=%s;]/%s\n",jobid, t_wnode, j2js[id], j2ec[id], pr_removal);
           }else{
            sprintf(out_buf,"[BatchJobId=\"%s\"; %s JobStatus=%s;]/%s\n",jobid, t_wnode, j2js[id], pr_removal);
           }
	   
	  } else {
	  
     	   GetEventsInOldLogs(logdate);
	   
     	   if(j2js[id]!=NULL){

            if((out_buf=malloc(STR_CHARS)) == 0){
             sysfatal("can't malloc out_buf in LookupAndSend: %r");
            }
	    
            if(strcmp(j2wn[id],"\0")==0){
             t_wnode[0]='\0';
            }else{
             sprintf(t_wnode,"WorkerNode=%s;",j2wn[id]);
            }
            if((strcmp(j2js[id],"3")==0) || (strcmp(j2js[id],"4")==0)){
             pr_removal="Yes";
            } else {
             pr_removal="Not";
            }
            if(strcmp(j2js[id],"4")==0){
             sprintf(out_buf,"[BatchJobId=\"%s\"; %s JobStatus=%s; ExitCode=%s;]/%s\n",jobid, t_wnode, j2js[id], j2ec[id], pr_removal);
            }else{
             sprintf(out_buf,"[BatchJobId=\"%s\"; %s JobStatus=%s;]/%s\n",jobid, t_wnode, j2js[id], pr_removal);
            }
	    
	   } else {
            if((out_buf=malloc(STR_CHARS)) == 0){
             sysfatal("can't malloc out_buf in LookupAndSend: %r");
            }
     	    sprintf(out_buf,"JobId %s not found/Not\n",jobid);
    	   }
	   
     	  }
     	  break;
	 } 
	 else {
	  sleep(1);
	 }
	   
        }
	
	if(i==WRETRIES){
         if((out_buf=malloc(STR_CHARS)) == 0){
          sysfatal("can't malloc out_buf in LookupAndSend: %r");
         }
	 sprintf(out_buf,"Cache locked/Not\n");
	}
close:	
 	Writeline(conn_s, out_buf, strlen(out_buf));
	
	free(out_buf);
	free(buffer);
	
	/*  Close the connected socket  */

	if ( close(conn_s) < 0 ) {
	    fprintf(stderr, "BLParserLSF: Error calling close()\n");
	    exit(EXIT_FAILURE);
	}
	
    } /* closes while */
    return(0); 
}

int GetEventsInOldLogs(char *logdate){

 char *loglist=NULL;
 
 loglist=GetLogList(logdate);
 if(loglist!=NULL){
  GetAllEvents(loglist);
 }
 
 return 0;
 
}

char *GetLogDir(){

 char *confpath;
 char *binpath;
 char *lsf_base_path;
 char conffile[STR_CHARS];
 char lsf_clustername[STR_CHARS];
 char ls_out[STR_CHARS];
 char *logpath;
 char *line;
 char command_string[STR_CHARS];
 int len;
 FILE *fp;
 FILE *file_output;
 FILE *ls_output;

 if((line=malloc(STR_CHARS)) == 0){
    sysfatal("can't malloc line: %r");
 }
 if((logpath=malloc(STR_CHARS)) == 0){
    sysfatal("can't malloc line: %r");
 }

 /*  setting defaults */
 
 if((binpath=getenv("LSF_BIN_PATH"))==NULL){
  binpath="/usr/local/lsf/bin/";
 }
 if((confpath=getenv("LSF_CONF_PATH"))==NULL){
  confpath="/etc";
 }

 sprintf(conffile,"%s/lsf.conf",confpath);
 
 if((fp=fopen(conffile, "r")) != 0){
  while(fgets(line, MAX_LINES, fp)){
   if(strstr(line,"LSB_SHAREDIR")!=0){
    break;
   }
  }
 } else {
  printf("Cannot open %s file\n",conffile);
  exit(-1);
 }

 lsf_base_path=strtok(line,"=");
 lsf_base_path=strtok(NULL,"=");
 strtok(lsf_base_path,"\n");
 
 sprintf(command_string,"ls %s/lsid 2>/dev/null",binpath);
 ls_output = popen(command_string,"r");
 if (ls_output != NULL){
  len = fread(ls_out, sizeof(char), sizeof(ls_out) - 1 , ls_output);
  if (len==0){
   printf("%s/lsid does not exist\n",binpath);
   printf("Change %s setting the env LSF_BIN_PATH\n",binpath);
   exit(-1);
  }
 }
 pclose(ls_output);

 sprintf(command_string,"%s/lsid | grep 'My cluster name is'|awk -F\" \" '{ print $5 }'",binpath);
 file_output = popen(command_string,"r");
 
 if (file_output != NULL){
  len = fread(lsf_clustername, sizeof(char), sizeof(lsf_clustername) - 1 , file_output);
  if (len>0){
   lsf_clustername[len-1]='\000';
  }
 }
 pclose(file_output);
 
 sprintf(logpath,"%s/%s/logdir",lsf_base_path,lsf_clustername);
 
return logpath;

}

char *GetLogList(char *logdate){
 
 char datefile[STR_CHARS];
 char touch_out[STR_CHARS];
 char rm_out[STR_CHARS];
 char logs[MAX_CHARS]="\0";
 char *slogs;
 char *t_logs;
 char tlogs[MAX_CHARS];
 char command_string[MAX_CHARS]="\0";
 int n=0;
 FILE *mktemp_output;
 FILE *touch_output;
 FILE *find_output;
 FILE *rm_output;
 FILE *ls_output;
 int len;
 int last_tag;

 if((slogs=malloc(MAX_CHARS)) == 0){
  sysfatal("can't malloc slogs: %r");
 }
 
 sprintf(command_string,"mktemp -q /tmp/blahdate_XXXXXX");
 mktemp_output = popen(command_string,"r");
 if (mktemp_output != NULL){
  len = fread(datefile, sizeof(char), sizeof(datefile) - 1 , mktemp_output);
  if (len>0){
   datefile[len-1]='\000';
  }
 }
 pclose(mktemp_output);
 
 sprintf(command_string,"touch -t %s %s",logdate,datefile);
 touch_output = popen(command_string,"r");
 if (touch_output != NULL){
  len = fread(touch_out, sizeof(char), sizeof(touch_out) - 1 , touch_output);
  if (len>0){
   touch_out[len-1]='\000';
  }
 }
 pclose(touch_output);
 
 if(LastLog!=NULL){
  sprintf(command_string,"find %s/%s.[0-9]* -type f -newer %s ! -newer %s -printf \"%%p \"", ldir, lsbevents, datefile, LastLog);
 } else{
  sprintf(command_string,"find %s/%s.[0-9]* -type f -newer %s -printf \"%%p \"", ldir, lsbevents, datefile);
 }
 
 find_output = popen(command_string,"r");
 if (find_output != NULL){
  len = fread(logs, sizeof(char), sizeof(logs) - 1 , find_output);
  if (len>0){
   logs[len-1]='\000';
  }
 }
 pclose(find_output);
  
 sprintf(command_string,"rm %s", datefile);
 rm_output = popen(command_string,"r");
 if (rm_output != NULL){
  len = fread(rm_out, sizeof(char), sizeof(rm_out) - 1 , rm_output);
  if (len>0){
   rm_out[len-1]='\000';
  }
 }
 pclose(rm_output);
 
 if(logs == NULL){
  return NULL;
 }
 sprintf(command_string,"ls -tr %s", logs);
 ls_output = popen(command_string,"r");
 if (ls_output != NULL){
  len = fread(tlogs, sizeof(char), sizeof(tlogs) - 1 , ls_output);
  if (len>0){
   tlogs[len-1]='\000';
  }
  pclose(ls_output);
 
  slogs[0]='\0';
  t_logs=strtok(tlogs,"\n");
  while(t_logs!=NULL){
   if(n==0){
    LastLog=strdup(t_logs);
    last_tag=n;
   }else if(n==1){
    last_tag=n; 
   }
   
   strcat(slogs,t_logs);
   strcat(slogs," ");

   t_logs=strtok(NULL,"\n");
   n++;
  }

/* last_tag is used to see if there is only one log file and to avoid to rescan it*/

  if(last_tag==0){
   return NULL;
  }
  return slogs;
  
 } else {
 
  return NULL;
  
 }
}

int strtoken(const char *s, char delim, char **token)
{
    char *tmp;
    char *ptr, *dptr;
    int i = 0;
    
    tmp = (char *) malloc(1 + strlen(s));
    assert(tmp);
    strcpy(tmp, s);
    ptr = tmp;
    while(1) {
        if((dptr = strchr(ptr, delim)) != NULL) {
            *dptr = '\0';
            token[i] = (char *) malloc(1 + strlen(ptr));
            assert(token[i]);
            strcpy(token[i], ptr);
            ptr = dptr + 1;
            i++;
        } else {
            if(strlen(ptr)) {
                token[i] = (char *) malloc(1 + strlen(ptr));
                assert(token[i]);
                strcpy(token[i], ptr);
                i++;
                break;
            } else{
	        break;
	    }
        }
    }
    
    token[i] = NULL;
    free(tmp);
    return i;
}

/* the reset is error processing stuff */

void
eprint(int err, char *fmt, va_list args)
{
    extern int errno;

    fprintf(stderr, "%s: ", argv0);
    if(fmt)
        vfprintf(stderr, fmt, args);
    if(err)
        fprintf(stderr, "%s", strerror(errno));
    fputs("\n", stderr);
    errno = 0;
}

char *
chopfmt(char *fmt)
{
    static char errstr[ERRMAX];
    char *p;

    errstr[0] = NUL;
    if((p=strstr(fmt, "%r")) != 0)
        fmt = strncat(errstr, fmt, p-fmt);
    return fmt;
}

/* syserror: print error and continue */
void
syserror(char *fmt, ...)
{
    va_list args;
    char *xfmt;

    va_start(args, fmt);
    xfmt = chopfmt(fmt);
    eprint(xfmt!=fmt, xfmt, args);
    va_end(args);
}

/* sysfatal: print error and die */
void
sysfatal(char *fmt, ...)
{
    va_list args;
    char *xfmt;

    va_start(args, fmt);
    xfmt = chopfmt(fmt);
    eprint(xfmt!=fmt, xfmt, args);
    va_end(args);
    exit(1);
}