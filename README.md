# lfs-pkgman
lfs-pkgman is a simple package manager for Linux From Scratch systems that can help you easily track installed packages and their versions so that they can be uninstalled/upgraded easily and safely
# Features
- Installs packages from package install directory(generated with a command like "make DESTDIR=package install") and tracks each file from the package
- Easily uninstalls packages by deleting each file they belong to
- Allows you to mark/unmark files as config files, so that marked files will not be overwritten by reinstalls of the same package
# How to build
Just run "make" and copy the generated binary into /usr/bin so that it can be used from anywhere
# Note
It's recommended to track base system packages with this tool if you plan to use it as a package manager, so that some program not part of the base system will not silently replace a system file, because this tool only prevents overwriting a package from a different tracked package.
# WARNINGS
- I have not yet tested that this package behaves properly on a normal system, but I did test the operations in a simulated root filesystem, so don't use this on a system that must remain stable(LFS shouldn't be used on systems that must remain stable anyway, but just putting accent on that)
- This tool should not be used on systems with an existing package manager as they can collide in weird untested ways
