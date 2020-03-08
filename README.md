# File-System-Driver
This is a custom file system driver created by Cameron Fitzpatrick  
and Hunter Kennedy at the University of California, Davis 2020.  

# Concept  
This is a custom virtual file system driver that you can use that  
is similar to a file system on MSFAT. Data is written to blocks of  
size 4096. The api is located in fs.h and is very similar to the  
file syscalls on C. For instance you can mount, unmount, open, close,  
read, write, etc. When writing to a file you can write past the file size.  


## To make a disk  
run `./fs_make.x <disk name> <disk size>`  

## To see the commands available  
run `./test_fs.x`  

## To use API  
You can see that it follows the API in fs.h  
by seeing the unit testing in progs. The API  
is located in fs.h and you can simply create a program  
in progs and use it. You MUST add this program to the  
makefile in progs, along with the other programs.  
As such:   `# Target programs
programs := test_fs.x \
	simple_test_fs.x \
  new_program1.x \
  new_program2.x \
  etc.` 
