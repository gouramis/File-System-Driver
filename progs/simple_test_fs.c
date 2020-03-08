#include <fs.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// File created by Cameron Fitzpatrick and Hunter Kennedy
static void test_mount_unmount(void){
	assert(-1 == fs_umount());
	fs_mount("disk.fs");
	assert(-1 == fs_mount("disk.fs"));
	assert(-1 == fs_mount("notafile.txt"));
	int fd = fs_open("asyoulik.txt");
	assert(-1 == fs_umount());
	assert(0 == fs_close(fd));
	assert(0 == fs_umount());
}

static void test_info() {
	assert(-1 == fs_info());
	fs_mount("disk.fs");
	assert(0 == fs_info());
	fs_umount();
	assert(-1 == fs_info());
	return;
}

static void test_create() {
	fs_mount("disk.fs");
	assert(-1 == fs_create(NULL));
	assert(-1 == fs_create("This is far too long of a filename for our program to work"));
	char buffer[11] = "file_x.txt";
	for(int i = 0; i < FS_FILE_MAX_COUNT - 1; i++) {
		char newbuffer[11];
		memcpy(newbuffer, buffer, 11);
		newbuffer[5] = i;
		assert(0 == fs_create(newbuffer));
	}
	assert(-1 == fs_create(buffer));
	for(int i = 0; i < FS_FILE_MAX_COUNT - 1; i++) {
		char newbuffer[11];
		memcpy(newbuffer, buffer, 11);
		newbuffer[5] = i;
		assert(0 == fs_delete(newbuffer));
	}
	fs_umount();
	return;
}

static void test_delete() {
	fs_mount("disk.fs");
	assert(-1 == fs_delete("This is far too long of a filename for our program to work"));
	assert(-1 == fs_delete(NULL));
	assert(-1 == fs_delete("NewFile.txt"));
	fs_create("NewFile.txt");
	int fd = fs_open("NewFile.txt");
	assert(-1 == fs_delete("NewFile.txt"));
	fs_close(fd);
	assert(0 == fs_delete("NewFile.txt"));
	fs_umount();
	return;
}

static void test_ls() {
	assert(-1 == fs_ls());
	fs_mount("disk.fs");
	assert(0 == fs_ls());
	fs_umount();
	return;
}

static void test_open() {
	assert(-1 == fs_open("asyoulik.txt"));
	fs_mount("disk.fs");
	assert(-1 == fs_open(NULL));
	assert(-1 == fs_open("This is far too long of a filename for our program to work"));
	for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		assert(-1 != fs_open("asyoulik.txt"));
	}
	assert(-1 == fs_open("asyoulik.txt"));
	for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		assert(0 == fs_close(i));
	}
	fs_umount();
}

static void test_close() {
	assert(-1 == fs_close(-1));
	assert(-1 == fs_close(33));
	assert(-1 == fs_close(0));
	fs_mount("disk.fs");
	int fd = fs_open("asyoulik.txt");
	assert(0 == fs_close(fd));
	assert(-1 == fs_close(fd));
	fs_umount();
	return;
}

static void test_stat() {
	fs_mount("disk.fs");
	assert(-1 == fs_stat(-1));
	assert(-1 == fs_stat(0));
	assert(-1 == fs_stat(33));
	int fd = fs_open("asyoulik.txt");
	assert(-1 != fs_stat(fd));
	fs_close(fd);
	assert(-1 == fs_stat(fd));
	fs_umount();
	return;
}

static void test_lseek() {
	assert(-1 == fs_lseek(10, 5));
	fs_mount("disk.fs");
	int fd = fs_open("asyoulik.txt");
	assert(-1 == fs_lseek(10, 5));
	assert(0 == fs_lseek(fd, 5));
	fs_close(fd);
	assert(-1 == fs_lseek(fd, 5));
	return;
}

static void test_write() {
	char buf[15] = "	AS YOU LIKE IT";
	assert(-1 == fs_write(10, buf, 0));
	fs_mount("disk.fs");
	assert(-1 == fs_write(10, buf, 0));
	int fd = fs_open("asyoulik.txt");
	assert(15 == fs_write(fd, buf, 15));
	assert(-1 == fs_write(fd, NULL, 15));
	fs_close(fd);
	assert(-1 == fs_write(fd, buf, 15));
	return;
}

static void test_read() {
	char buf[10];
	size_t size = 10;
	assert(-1 == fs_read(-1, buf, size));
	assert(-1 == fs_read(0, buf, size));
	assert(-1 == fs_read(33, buf, size));
	fs_mount("disk.fs");
	int fd = fs_open("asyoulik.txt");
	assert(10 == fs_read(fd, buf, size));
	fs_close(fd);
	assert(-1 == fs_read(fd, buf, size));
	return;
}

int main() {
	test_mount_unmount();
	test_info();
	test_create();
	test_delete();
	test_ls();
	test_open();
	test_close();
	test_stat();
	test_lseek();
	test_write();
	test_read();
	return 0;
}
