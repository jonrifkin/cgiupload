//----------------------------------------------------------------------
//   About
//----------------------------------------------------------------------
//
//  This is a script written in C that works as a CGI script with Apache2.
//  It performs a quick and efficient multiple file uploads.
//
//  IMPORTANT:  To use, first you must recompile with your local values
//              for PATH_UPLOAD_DIR and LOGDIR (see below).
//

//----------------------------------------------------------------------
//   History
//----------------------------------------------------------------------

//  2016-01-23  Started
//  2016-01-26  First working version


//----------------------------------------------------------------------
//   Includes
//----------------------------------------------------------------------

//  Use GNU specific memmem() function to search for string in memory
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>


//----------------------------------------------------------------------
//   Configuration
//----------------------------------------------------------------------


/*
#ifndef PATH_UPLOAD_DIR
#error  To compile cgiupload.c, you must define PATH_UPLOAD_DIR in the Makefile
#endif

#ifndef PATH_LOG_FILE
#error  To compile cgiupload.c, you must define PATH_LOG_FILE in the Makefile
#endif
*/
// #define PATH_UPLOAD_DIR /export/www/sites/signtyp/WorkArea/Upload/files
// #define PATH_LOG_FILE   /export/www/sites/signtyp/WorkArea/Upload/log/upload.log


#define BUFFLEN  0x400000  //  4MB input buffer
#define BARRLEN  256       //  Maximum length of barrier string, which signals start of a file
#define LINELEN  256       //  Maximum length of one header line, and filename
#define PATHLEN  1024      //  Maximum length of file path



//----------------------------------------------------------------------
//   Functions
//----------------------------------------------------------------------

//  Print HTTP starting Content-type header
void start_http() {
	printf("Content-type: text/plain\n\n");
}

//  Print message with HTTP-required Content-type header, and exit
void msg_and_exit(char *format, ...) {
	va_list aptr;
	va_start(aptr, format);
	printf(format, aptr);
	va_end(aptr);
	printf("\n");
	exit(0);
}

//  Returns true if string starts with prefix
int startswith(char *string, char *prefix) {
	int plen = strlen(prefix);
	//  False, because prefix is longer than string
	if (strlen(string)<plen) return 0;
	//  True
	if (0==memcmp( (void *) string, (void *) prefix, plen)) return 1;
	//  False
	return 0;
}

//  Returns true if string ends with suffix
int endswith(char *string, char *suffix) {
	int slen = strlen(suffix);
	int spos = strlen(string) - slen;
	//  False, because suffix is longer than string
	if (spos<0) return 0;
	//  True
	if (0==memcmp( (void *) (string+spos), (void *) suffix, slen)) return 1;
	//  False
	return 0;
}

//  Return time stamp conforming to timeformat
char *get_timestamp(char *timeformat) {
	static char timestamp[LINELEN];
	time_t t = time(NULL);
	struct tm *tmp = localtime(&t);
	strftime(timestamp, LINELEN, timeformat, tmp);
	return timestamp;
}


// Make new filename by prepending time stamp
//   new_filename is buffer nchars long
void make_new_filename(char *filename, char *new_filename, int nchars) {
	new_filename[0] = '\0';
	strncat(new_filename,get_timestamp("%Y%m%d-%H%M%S-"),nchars);
	strncat(new_filename,filename,nchars-strlen(new_filename)-1);
	//  Change dangerous suffixes to .txt
	char *pext = strrchr(new_filename, '.');
	//  No suffice, so return
	if (pext==NULL)  return;
	pext++;
	if  ( ( 0==strcmp(pext,"php")) || (0==strcmp(pext,"php3")) || (0==strcmp(pext,"phtml")) || (0==strcmp(pext,"html")) || (0==strcmp(pext,"js"))  ) {
		pext[0] = '\0';
		strncat(new_filename,"txt",3);
	}
}

//  Look for value of data associated with key.  For example, if line is
//    'Content-Disposition: form-data; name="movie"; filename="2.JPG"'
//  the key 'filename' will return the value '2.JPG'
//  
void copy_token(char *line, char *key, char *value, int vlen) {
	//  Find key string within line
	char *pos = strstr(line,key);
	//  Initialze value to null string
	value[0] = '\0';
	//  key not found, return an empty value
	if (pos==NULL) return;
	// Find position of value
	pos += strlen(key);
	//  Remaining string is only three characters long, 
	//  too short to contain =".." where .. is non-empty string
	if (strlen(pos)<=3) return;
	//  Set position to start of value
	pos += 2;
	//  Find trailing double-quote
	char *lpos = strchr(pos,'"');
	//  No double-quote found, return empty string
	if (lpos==NULL) return;
	//  Calculate length of string
	int len = lpos-pos;
	//  Too long, return empty string
	if (len>=vlen) return;
	//  Copy found value to returning string
	memcpy(value,pos,len);
	value[len] = '\0';
}

//  Append nchars from *ptr to *str, up to a total of strsize-1 chars. Surplus
//  chars are silently dropped.
//  Return the new string length.
int str_append(char *str, int lstr, int strsize, char *ptr, int nchars) {
	int strfree = strsize-lstr-1;  //  One less to store terminating null byte
	if (strfree) {
		int ncopy = strfree < nchars ? strfree : nchars;
		memcpy(str+lstr,ptr,ncopy);
		lstr += ncopy;
		str[lstr] = '\0';
	}
	return lstr;
}


//  Read stdin until barrier strings is found, and transfer chars to
//  either a string or an output file.
//    CASE - NO DATA WRITTEN
//    - If str is empty and strsize is zero, then chars read are
//      dropped.
//    CASE - DATA WRITTEN TO FILE
//    - If strsize is zero, then str holds the output file name, and
//      reads read from stdin are written to the file.  Any previous
//      file content is lost.  Also for files, the barrier string and
//      the final two characters before the barrier string, are thrown
//      away.  This is because for file uploads, the two characters
//      before the barrier strings are "\r\n", which are inserted by
//      the web client software.
//    CASE - DATA WRITTEN TO STRING
//    - If strsize is greater than zero, then str is a buffer that will
//      hold the chars read from stdin.  The barrier string is included
//      in the string, unless the string buffer provied is too small.
//      Any input chars read that don't fit in the string are thrown
//      away. 
size_t transfer_buffered_stdin(char *buf, int *cur, int *end, int bufsize, char *barrier, char *str, int strsize) {
	if (!barrier) msg_and_exit("ERROR:  Barrier string is empty");
	//  Lremain is the number of chars to leave at the end of the buffer,
	//  because they may contain part of the barrier string.
	int lremain = strlen(barrier) - 1;
	FILE *fout = (FILE *) NULL;
	//  Open file
	if (strsize==0) {
		//  No file, so don't write data, just skip it (for example, after a 'submit'header)
		if (str[0]=='\0')  {
			fout = NULL;
		} else  {
			fout = fopen(str,"w");
			if (! fout) msg_and_exit("ERROR:  Cannot open file %s/%s for writing: %s", PATH_UPLOAD_DIR, str, strerror(errno) ); 
		}
	//  Initialize string
	} else {
		str[0] = '\0';
	}
	//  Repeat until barrier found or stdin empty
	size_t nbytes = 0;
	while (1) {
		//  Refill buffer if number of remaining chars is less than barrier length
		int nremain = *end - *cur;
		if (nremain<=lremain) {
			// Move remaining chars to start of buffer
			memmove(buf,buf+*cur,nremain);
			// Fill remaining buffer
			int nread = fread(buf+nremain,1,bufsize-nremain,stdin);
			//  Reset pointers
			*cur = 0;
			*end = nremain + nread;
			//  EOF reached on stdin - write or return remaining chards
			if (nread==0) {
				//  Store chars in string
				if (strsize) {
					str_append(str, 0, strsize, buf, nremain);
					nbytes = nremain;
				//  Write file
				} else {
					if (fout) fwrite(buf,1,nremain,fout);
					nbytes = nremain;
				}
				break;
			}
		}
		//  Search buffer, starting at cursor, for barrier string
		char *pos = (char *) memmem( buf+*cur, *end-*cur, barrier, lremain+1);
		//   Barrier string found - save chars and return
		if (pos) {
			//  Store chars in string
			if (strsize) {
				//  When writing to string, include the barrier
				int ncopy = pos - (buf+*cur) + lremain+1;
				str_append(str, strlen(str), strsize, buf+*cur, ncopy);
				nbytes += ncopy;
			//  Write char data to file
			} else {
				//  When writing to file, omit the barrier
				//    Truncate last two characters, because they are "\r\n"
				//    and were added by the web client
				if (fout) fwrite( buf+*cur, 1, pos-(buf+*cur)-2, fout );
				nbytes += pos-(buf+*cur)-2;
			}
			//  Advance cursor past the barrier string
			*cur = pos-buf+lremain+1;
			break;
		//  Barrier not found
		} else {
			//  Write first characters into string, drop those that don't fit
			if (strsize) {
				int ncopy = *end - *cur - lremain;
				str_append(str, strlen(str), strsize, buf, ncopy);
				nbytes += ncopy;
			//  Write char data to file
			} else {
				if (fout) fwrite( buf+*cur, 1, *end-*cur-lremain, fout );
				nbytes += BUFFLEN-lremain;
			}
			//  Advance cursor to start of remaining characters
			*cur = *end - lremain;
		}
	}
	//  Close file
	if (strsize==0 && fout) fclose(fout);

	//  Return number of bytes read;
	return nbytes;
}


//  Write log message.  We open/close the file each time this is called,
//  because calls to write_log can be far apart in time if uploaded
//  files are large.
void write_log(char *msg) {

	//  Open log
	FILE *fout = fopen(PATH_LOG_FILE,"a");

	//  Write time stamp
	char *t = get_timestamp("%Y-%m-%d %H:%M:%S ");
	fwrite(t,1,strlen(t),fout);
	//  Write ip address
	t = getenv("REMOTE_ADDR");
	if (t) fwrite(t,  1,strlen(t),fout);
	else   fwrite("-",1,1,        fout);
	fwrite(" ",1,1,fout); //  Write separator space
	//  Write msg
	fwrite(msg,1,strlen(msg),fout);
	fwrite("\n",1,1,fout);  //  Write end of line

	//  Close log
	fclose(fout);
}

//----------------------------------------------------------------------
//   Main
//----------------------------------------------------------------------

int main(int argc, char *argv[]) {
	
	//  Write Content-type for web client
	start_http();

	//  Buffers for various info
	char *buf          =  (char *) malloc(BUFFLEN);
	char *barrier      =  (char *) malloc(BARRLEN);
	char *line         =  (char *) malloc(LINELEN);
	char *filename     =  (char *) malloc(LINELEN);
	char *new_filename =  (char *) malloc(LINELEN);
	char *curdir       =  (char *) malloc(PATHLEN);

	//  Save original working directory
	if (!getcwd(curdir, PATHLEN)) msg_and_exit("ERROR:  Cannot get value of starting directory");

	//  Change to output directory
	if (chdir(PATH_UPLOAD_DIR)) msg_and_exit("ERROR:  Cannot change to directory %s.", PATH_UPLOAD_DIR);

	//  First line from stdin, this is the 'barrier' string
	fgets(barrier, BARRLEN, stdin);
	int lbarrier = strlen(barrier);

	//  Read headers and file content until EOF
	int cur = 0;
	int end = 0;
	int nfiles = 0;
	size_t tbytes = 0;
	while (1) {

		int start_time = time(NULL);

		//   Read headers,  and save the filename found there
		filename[0] = '\0';
		new_filename[0] = '\0';
		while (1) {

			//  Read header lines (they end in "\r\n", so that's
			//  the value we use for the barrier string)
			transfer_buffered_stdin(buf, &cur, &end, BUFFLEN, /*barrier*/ "\r\n", line, LINELEN);

			//  End-of-header found, continue on write to file
			if (strlen(line)==2) break;

			//  Last file was read - end program
			if (line[0]=='\0') goto END;

			//  Look for Content-Disposition header to obtain output filename
			if (startswith(line,"Content-Disposition")) {
				//  Find value that follows 'filename': like this 'filename="XXX"'
				copy_token(line,"filename",filename,LINELEN);
				//  Make new filename by prepending timestamp
				make_new_filename(filename, new_filename, LINELEN);
			}

		}  //  loop until end-of-header found

		//  File present, so proces it
		if (filename[0]) {

			//  Transfer chars to file
			size_t nbytes = transfer_buffered_stdin(buf, &cur, &end, BUFFLEN, barrier, new_filename, 0);

			//  Print file name
			if (filename[0]) {
				printf("File '%s' stored as '%s'\n", filename, new_filename); 
				printf("File size %ld\n", nbytes);
				printf("\n");
				nfiles++;
				tbytes += nbytes;
			}

			//  Log results
			snprintf(line, LINELEN, "uploaded file %s, %ld bytes, stored as %s/%s (upload took %ld seconds)",
				filename,
				nbytes,
				PATH_UPLOAD_DIR,
				new_filename,
				time(NULL) - start_time
			);
			write_log(line);
		}
	}


	//  Clean-up
END:	
	printf("\n");
	printf("Total of %d files written (total of %ld bytes)\n", nfiles, tbytes);

	//  Restore original directory
	chdir(curdir);

	//  Free storage
	free(curdir);
	free(new_filename);
	free(filename);
	free(line);
	free(barrier);
	free(buf);


	return 0;
}
