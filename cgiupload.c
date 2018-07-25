//----------------------------------------------------------------------
//   About
//----------------------------------------------------------------------

//----------------------------------------------------------------------
//   History
//----------------------------------------------------------------------
//  2016-01-23  Started

//----------------------------------------------------------------------
//   Includes
//----------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
//  Use GNU specific memmem() function to search for string in memory
#define _GNU_SOURCE
#include <string.h>

//----------------------------------------------------------------------
//   Configuration
//----------------------------------------------------------------------

#define OUTDIR "/export/tmp"

#define BUFFLEN  0x400000  //  4MB input buffer
#define BARRLEN  256       //  Maximum length of barrier string, which signals start of a file
#define LINELEN  256       //  Maximum length of header line


//----------------------------------------------------------------------
//   Functions
//----------------------------------------------------------------------

void msg_and_exit(char *msg) {
	printf("Content-type: text/plain\n\n%s\n", msg);
	exit(0);
}

int startswith(char *string, char *prefix) {
	int plen = strlen(prefix);
	//  False, because prefix is longer than string
	if (strlen(string)<plen) return 0;
	//  True
	if (0==memcmp( (void *) string, (void *) prefix, plen)) return 1;
	//  False
	return 0;
}

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
	if (lpos==NULL) return;
	//  Temporarily write NULL byte to signal end of string
	*lpos = '\0';
	if (strlen(pos)>vlen-1) return;
	strcpy(value,pos);
	//  Reset value at lpos
	*lpos='"';
	/*test*/ printf("test: copy_token: line key value (%s) (%s) (%s)\n", line, key, value);
}

//  Read next \r\n terminated line from input buffer rptr, write into wptr
//  Return pointer to input buffer just after string.
char *read_buf_newline(char *rptr, char *wptr, int maxwlen) {
	//  Find newline
	char *end = memmem(rptr,maxwlen-2,"\r\n",2);
	//  Found \r\n terminated string: copy to write buffer and move read buffer pointer
	if (end) {
		int l = end-rptr+2;
		memcpy(wptr,rptr,l);
		return rptr+l;
	} else {
		return (char *) NULL;
	}
}

//  Write to output file, and re-fill buffer from stdin, until barrier string
//  found, or until end of input.  Return pointer to just past barrier.  Null
//  pointer means of stdin.
char write_until_barrier(char *buffer, char *beg, char *end, int lbuffer, char *barrier, int lbarrier, FILE *fin, FILE *fout) {
	//  Find barrier
	char *p = memmem(beg,end-beg,barrier,lbarrier);
	//  Barrier found, write until end of barrier 
	if (p) {
		fwrite(beg,p-beg,fout);
		return p+lbarrier;
	//  Barrier string not found, write until end
	}
	while(1) {
		//  Write buffer, excluing last few characters where may hold partial barrier string
		fwrite(beg,1,end-lbarrier+1,fout);
		//  Move partial barrier string to start of buffer
		memmove(buffer,end-lbarrier+1,lbarrier-1);
		//  Re-fill buffer
		int nread = fread(buffer+lbarrier-1,1,BUFFLEN-lbarrier+1,fin);
		//  End of input
		if (nread==0) return (char *) NULL;
	}
}


//----------------------------------------------------------------------
//   Main
//----------------------------------------------------------------------

// Content-Disposition: form-data; name="movie"; filename="2.JPG"

int main(int argc, char *argv[]) {

	//  Buffer can contain residue from previous buffer (up to length BARRLEN), 
	//  content read from stdin (len BUFFLEN)
	char *buf     =  (char *) malloc(BARRLEN+BUFFLEN);
	char *barrier =  (char *) malloc(BARRLEN);
	char *line    =  (char *) malloc(LINELEN);
	char *fname   =  (char *) malloc(LINELEN);
	char *residue =  (char *) malloc(BARRLEN);

	char *bufend  =  buf+BARRLEN+BUFFLEN;

	//  First line is the 'barrier' string
	fgets(barrier, BARRLEN, stdin);
	int lbarrier = strlen(barrier);

	//  No barrier string found (because \r\n not terminating string
	if (barrier[l-1]!='\n' || barrier[l-2]!='\r') {
		msg_and_exit("ERROR:  Barrier string not found at start of buffer");
	}

	//  residue	
	residue[0] = '\0';
	size_t offset = 0;

	//  Pre-load buffer
	nread = fread(buf+BARRLEN, 1, size(BUFFLEN), stdin);
	char *nextptr = buf+BARRLEN;

	//  Read all files from stdin
	while (1) {


		//  Read headers from buffer
		fname[0] = '\0';
		int cnt = 0;
		while(1) {
			char *nextptr = read_buf_newline(nextptr, line, LINELEN);
			if (!nextptr) {
				msg_and_exit("ERROR:  Incomplete data header read");
			}
			cnt += 1;
			if (cnt>40) {
				msg_and_exit("ERROR:  Data header unexpectedly long, more than 40 lines");
			}
			//  Look for Content-Disposition header
			if (startswith(line,"Content-Disposition")) {
				copy_token(line,"filename",fname,LINELEN);
				if (fname[0]=='\0') {
					msg_and_exit("ERROR:  Cannot read filename from Content-Disposition line in header");
				}
			}
			//  Header terminating line found
			if (2==strlen(line)) break;
		}
		
		//  No target filename found
		if (fname[0]=='\0') {
			msg_and_exit("ERROR:  No input file name found in Content-Disposition header");
		}

		//  If target file name ends with forbidden suffix, add a .txt to the end
		if (
			endswith(fname,".php") || 
			endswith(fname,".php5") || 
			endswith(fname,".htm") || 
			endswith(fname,".html") || 
			endswith(fname,".htmlp") || 
			endswith(fname,".html5") 
		) {
			//  Filename not long enough to hold added .txt suffix
			if (strlen(fname)+4>LINELEN) {
				msg_and_exit("ERORR:  Upload file name is too long");
			}
			strcat(fname,".txt");
		}

		//  Open next output file
		FILE *fout = fopen(fname,"w");

		//  Search buffer for ending barrier string
		char *barrptr = memmem(nextptr, bufend-nextptr, barrier, lbarrier);
		if 

		//  Transfer binary content to target file
		size_t nread;
		while (1) {
			nread = fread(buf,1,BUFLEN,stdin);
			if (nread==0) goto END_OF_FILE;
			fgets(buf+BARRLEN,BUFFLEN,stdin);
			/*  Search for next barrier  */
			char *next = memmem(buf+BARRLEN-strlen(residue),nread,barrier,lbarrier);
			if (!next) {
				fwrite(buf+BARRLEN,1,nread,fout);
				continue;
			} else {
				fwrite(buf+BARRLEN,1,next-buf-BARRLEN,fout);
				fclose(fout);
			}
		
		}

	}


	END_OF_FILE: 

	//  Free strings
	free(buf);
	free(barrier);
	free(line);
	free(fname);
	free(residue);
	
	exit(0);
}

