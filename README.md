# TracerHid 

Hides `TracerPid` by setting it to zero in every `/proc/(pid)/status` file.

## System Requirements
* 64bit Linux system
* Kernel Version >= 3.19( for FTRACE_OPS_FL_IPMODIFY )
* The following Kernel Build configurations 
	* CONFIG_FTRACE
	* CONFIG_KALLSYMS
	* CONFIG_DYNAMIC_FTRACE_WITH_REGS
	* CONFIG_HAVE_FENTRY
	
Your distro provider probably put a list of your config options in `/boot/config*`, there's a good chance your kernel already has these options, but if it does not, you'll have to rebuild from source.
* Kernel headers for your current kernel.
* elfutils development package ( "elfutils-libelf-devel" for redhat, "libelf-dev" for ubuntu )
* Development Essentials ( make, gcc, etc. )
## Build Instructions
*  After installing kernel headers, you should just be able to use the makefile.
* `make` in the main directory.

## How to Use
After you are done building, you will need to load it. Since it is a kernel module, it has to be done by root.

`sudo insmod tracerhid.ko`

You can see all the output TracerHid makes in the kernel log with `dmesg --follow` or if you don't have dmesg, `tail -f` the appropriate log in /var/log

To unload the module, use `sudo rmmod tracerhid`. System behavior will return to normal.

## Credits

-Alexey Lozovsky - For his series of articles [part1](https://www.apriorit.com/dev-blog/544-hooking-linux-functions-1) about ftrace and hooking with ftrace along with code snippets that I used in this project.

-Cheddar Cheeze - Idea