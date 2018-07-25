cgiupload: cgiupload.c
	cc -g -o cgiupload cgiupload.c

clean:
	rm cgiupload

test: cgiupload
	./cgiupload < upload_raw
