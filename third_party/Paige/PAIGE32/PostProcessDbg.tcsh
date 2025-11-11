#!../../BinTools/tcsh

if ( $?EudoraBuildVerbosity == 0 ) then
    @ EudoraBuildVerbosity = 1
endif

if ( $EudoraBuildVerbosity >= 2 ) then
echo .
echo "Running $0 on $1"
echo .
endif

setenv PATH "..\..\BinTools;${PATH}"

if ( $EudoraBuildVerbosity >= 3 ) then
echo Win95 command.com is broken. Using tcsh to do post-processing.
echo UNIX tools come to the rescue.
endif

# Ensure that the destination directories exist.
if ( $EudoraBuildVerbosity >= 2 ) then
echo "......................................................"
echo ".                                                    ."
echo ". Ensuring that destination directories exist.       ."
echo ".                                                    ."
echo "......................................................"
endif

if ( ! -d "../../Lib" ) then
    mkdir ../../Lib
endif

if ( ! -d "../../Lib/Debug" ) then
    mkdir ../../Lib/Debug
endif

if ( ! -d "../../Bin" ) then
    mkdir ../../Bin
endif

if ( ! -d "../../Bin/Debug" ) then
    mkdir ../../Bin/Debug
endif

# Ensure that the destination file are writable.
if ( $EudoraBuildVerbosity >= 2 ) then
echo "......................................................"
echo ".                                                    ."
echo ". Ensuring that destination file(s)                  ."
echo ".                                                    ."
echo ".          ../../Lib/Debug/$1.lib"
echo ".          ../../Bin/Debug/$1.dll"
echo ".                                                    ."
echo ". are writable.                                      ."
echo ".                                                    ."
echo "......................................................"
endif
if ( -f "../../Lib/Debug/$1.lib" ) then
    chmod 777 ../../Lib/Debug/$1.lib
endif
if ( -f "../../Bin/Debug/$1.dll" ) then
    chmod 777 ../../Bin/Debug/$1.dll
endif

# Copy the appropriate files.
if ( $EudoraBuildVerbosity >= 2 ) then
echo "......................................................"
echo ".                                                    ."
echo ". Copying                                            ."
echo ".                                                    ."
echo ".          Build/Debug/$1.lib"
echo ".                                                    ."
echo ". to                                                 ."
echo ".          ../../Lib/Debug                           ."
echo "......................................................"
endif
if ( -f "Build/Debug/$1.lib" ) then
    cp Build/Debug/$1.lib ../../Lib/Debug
else
    echo "%% ERROR: Source file Build/Debug/$1.lib missing."
endif

# Copy the appropriate files.
if ( $EudoraBuildVerbosity >= 2 ) then
echo "......................................................"
echo ".                                                    ."
echo ". Copying                                            ."
echo ".                                                    ."
echo ".          Build/Debug/$1.dll"
echo ".                                                    ."
echo ". to                                                 ."
echo ".          ../../Bin/Debug                           ."
echo "......................................................"
endif
if ( -f "Build/Debug/$1.dll" ) then
    cp Build/Debug/$1.dll ../../Bin/Debug
else
    echo "%% ERROR: Source file Build/Debug/$1.dll missing."
endif

# Copy the appropriate files.
if ( $EudoraBuildVerbosity >= 2 ) then
echo "......................................................"
echo ".                                                    ."
echo ". Copying                                            ."
echo ".                                                    ."
echo ".          Build/Debug/$1.pdb                        ."
echo ".                                                    ."
echo ". to                                                 ."
echo ".          ../../Bin/Debug                           ."
echo "......................................................"
endif
if ( -f "Build/Debug/$1.pdb" ) then
    cp Build/Debug/$1.pdb ../../Bin/Debug
else
    echo "%% ERROR: Source file Build/Debug/$1.pdb missing."
endif
