#  Set location for file upload directory and log file
PATH_UPLOAD_DIR = test
PATH_LOG_FILE = test.log

#  Note that we place defined strings inside double-quotes - this is necessary for the C compiler to make sense of it
cgiupload: cgiupload.c
	cc -DPATH_UPLOAD_DIR=\"$(PATH_UPLOAD_DIR)\" -DPATH_LOG_FILE=\"$(PATH_LOG_FILE)\" -g -o cgiupload cgiupload.c

clean:
	rm -f cgiupload upload_raw test/*
	rmdir test

test:
	@mkdir -p test
	@./mktestinput.sh  cat.jpg image/jpeg  slidewhistle.mp3 audio/mp3 > test/upload_raw
	@cc -DPATH_UPLOAD_DIR=\"test\" -DPATH_LOG_FILE=\"test.log\" -g -o test/cgiupload cgiupload.c
	@./test/cgiupload < test/upload_raw > /dev/null
	@diff cat.jpg test/*cat.jpg && diff slidewhistle.mp3 test/*slidewhistle.mp3 && echo PASSED!

