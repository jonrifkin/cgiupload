#  Set location for file upload directory and log file
PATH_UPLOAD_DIR = test
PATH_LOG_FILE = test.log

#  Note that we place defined strings inside double-quotes - this is necessary for the C compiler to make sense of it
cgiupload: cgiupload.c
	cc -DPATH_UPLOAD_DIR=\"$(PATH_UPLOAD_DIR)\" -DPATH_LOG_FILE=\"$(PATH_LOG_FILE)\" -g -o cgiupload cgiupload.c

clean:
	rm cgiupload

test:
	mkdir -p test
	./cgiupload < upload_raw
