#!/bin/bash

###################################################################################
# Assignment 1
# Author   - Ananth Deshpande
# email id - ande9392@colorado.edu
# filename - writer.sh
###################################################################################

#to copy arguments to local variables
writefile=$1
writestr=$2

#to check if both arguments were provided
if [ $# -ne 2 ]
then
   echo please provide two arguments as described below:
   echo 1st arg must be a file path
   echo 2nd arg must be string to be written to the file
   exit 1
fi

#to create folder path if it doesnt exist
#https://stackoverflow.com/questions/9452935/unix-create-path-of-folders-and-file
mkdir -p "$(dirname "$writefile")"

#to create file in provided path or to overwrite a file if it already exists
touch "$writefile"

#to check if the file was successfully created, if not print an error message
#and exit 1
if [ $? -ne 0 ]
then
   echo file creation failed
   exit 1
fi

#write the string to respective file
echo "$writestr" > "$writefile"

#################################################################################
