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
- I have not yet tested that this package behaves properly on a normal system, but I did test the operations in a simulated root filesystem, so use this at your own risk
- You shouldn't install Glibc with this tool as it can corrupt the entire system. It's recommended to install Glibc the normal way, and optionally create a metadata file manually:
```bash
make DESTDIR=/tmp/glibc-build install
mkdir -p /var/lib/lfs-pkgman/packages
echo <Glibc version> > /var/lib/lfs-pkgman/packages/glibc
cd /tmp/glibc-build
find . ! -type d | sed 's|^\.||' >> /var/lib/lfs-pkgman/packages/glibc
rm -rf /tmp/glibc-build
```
Make sure to also add the contents of the 32-bit version if building Multilib Linux From Scratch
- This tool should not be used on systems with an existing package manager(except pip which is also in LFS) as they can collide in weird untested ways
- You shouldn't track Python packages with this tool, as pip already does a good enough job and can cause issues while providing no extra help
