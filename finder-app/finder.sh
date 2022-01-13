#!/bin/bash

##################################################################################################
# Assignment 1
# Author   - Ananth Deshpande
# email id - ande9392@colorado.edu
# filename - finder.sh
##################################################################################################

#to obtain input arguments into local variables 
filesdir=$1
searchstr=$2

#to check if both arguments were provided
if [ $# -ne 2 ]
then
   echo please provide two arguments as described below:
   echo 1st arg must be a directory
   echo And 2nd arg must be a search string
   exit 1
fi

#to check if the first argument provided was a directory
if [ ! -d "$filesdir" ]
then
   echo please mention a file directory as first arg
   exit 1
fi

#to obtain number of files in the respective directory
#and all subdirectories
noOfFilesInfilesdir=$(find "$filesdir" -type f | wc -l)

#to find all the matching lines in provided directory
noOfMatchingLinesInFiles=$(grep -r "$searchstr" "$filesdir" | wc -l)

echo The number of files are $noOfFilesInfilesdir and the number of matching lines are $noOfMatchingLinesInFiles

###################################################################################################
