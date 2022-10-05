# CGIUPLOAD

A cgi script written in C for uploading files to a web server.
Tested with the Apache2 web server.

# WHAT CGIUPLOAD DOES

Cgiupload is a cgi script that works with a web server, such as
Apache, and provides the ability for users to upload multiple files
to the server.

An example HTML file is include to show what HTML elements to place
in your web page to work with the script.

When cgiupload is called from the browser (using the example HTML),
the file(s) selected by the user are sent to the webserver.  The
cgiupload script will write the files to the `PATH_UPLOAD_DIR` and log
the transaction in `PATH_LOG_FILE`.  Cgiupload will then send a simple
message back to browser confirming the upload.

Note that when it writes the files to the upload directory, it
changes their names to prevent them from overwriting previously
existing files; it prepends the date and time to each file to make it
unique.  So, the file `cat.jpg` may become `20221005-121249-cat.jpg'
(the prefix format is `YYYYmmdd-HHMMSS-`).


# HOW TO COMPILE

1. Before you compile, you must decide where the uploaded files will be
written, and where to write the log file.   These must be set
using the variables `PATH_UPLOAD_DIR` and `PATH_LOG_FILE` in the
Makefile.  For example:
```
  PATH_UPLOAD_DIR = /var/upload
  PATH_LOG_FILE = /var/log/upload.log
```

Make sure that the upload directory and the log file are writeable by
the webserver.

NOTE: If you use relative path names for the above variables,
then `PATH_UPLOAD_DIR` will be be relative to the directory where
cgiupload is placed, and `PATH_LOG_FILE` will be relative to
`PATH_UPLOAD_DIR`.
       
2. To compile, simply run
```
   $ make 
```
The binary will be named cgiupload. 

# HOW TO TEST

Run the following command
```
      $ make test
```
If the test passed, you will see `PASSED!` written to the screen .


The test does the following

1. Creates a local directory called test

2. Compiles a new instance of cgiupload which is configured to 
   write its output into the test directory

3. Writes this new instances of cgiupload into the test directory

4. Combines the two included example files (`cat.jpg` and `slidewhistle.mp3`)
   into the format that a browser normally sends to the webserver, and
   writes this to the file upload_raw.

5. Runs cgiupload using the input just created.  This will write copies
   of the `cat.jpg` and `slidewhistle.mp3` the test directory.

6. Finally, the test compares the file copies with the originals. 


# HOW TO INSTALL

Copy the file cgiupload to the cgi directory on your web server.
