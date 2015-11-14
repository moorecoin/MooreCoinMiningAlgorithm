# building leveldb on windows

## prereqs 

install the [windows software development kit version 7.1](http://www.microsoft.com/downloads/dlx/en-us/listdetailsview.aspx?familyid=6b6c21d2-2006-4afa-9702-529fa782d63b).

download and extract the [snappy source distribution](http://snappy.googlecode.com/files/snappy-1.0.5.tar.gz)

1. open the "windows sdk 7.1 command prompt" :
   start menu -> "microsoft windows sdk v7.1" > "windows sdk 7.1 command prompt"
2. change the directory to the leveldb project

## building the static lib 

* 32 bit version 

        setenv /x86
        msbuild.exe /p:configuration=release /p:platform=win32 /p:snappy=..\snappy-1.0.5

* 64 bit version 

        setenv /x64
        msbuild.exe /p:configuration=release /p:platform=x64 /p:snappy=..\snappy-1.0.5


## building and running the benchmark app

* 32 bit version 

	    setenv /x86
	    msbuild.exe /p:configuration=benchmark /p:platform=win32 /p:snappy=..\snappy-1.0.5
		benchmark\leveldb.exe

* 64 bit version 

	    setenv /x64
	    msbuild.exe /p:configuration=benchmark /p:platform=x64 /p:snappy=..\snappy-1.0.5
	    x64\benchmark\leveldb.exe

