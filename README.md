# FAT-Based-File-System

The objectives of this programming project are:

- Implementing an entire FAT-based filesystem software stack: from mounting and unmounting a formatted partition, to reading and writing files, and including creating and removing files.

- Understanding how a formatted partition can be emulated in software and using a simple binary file, without low-level access to an actual storage device.

- Learning how to test your code, by writing your own testers and maximizing the test coverage.

- Writing high-quality C code by following established industry standards.

For this project, the specifications for a very simple file system have been defined and the layout of it on a disk is composed of four consecutive logical parts:

The Superblock is the very first block of the disk and contains information about the file system (number of blocks, size of the FAT, etc.)
The File Allocation Table is located on one or more blocks, and keeps track of both the free data blocks and the mapping between files and the data blocks holding their content.
The Root directory is in the following block and contains an entry for each file of the file system, defining its name, size and the location of the first data block for this file.
Finally, all the remaining blocks are Data blocks and are used by the content of files.

The size of virtual disk blocks is 4096 bytes.
