bootimg
=======

Splitting and Packing Android Boot images

Disclaimer
----------

I have tested these programs using [factory images for Nexus 4, 5 and
7](https://developers.google.com/android/nexus/images), but do not warrant
their use. Use of these programs will invalidate any warranty for
your device and may possibly brick your device.

*USE AT YOUR OWN RISK !*

Attribution
-----------

spbootimg.c (split boot image) and pkbootimg.c (pack boot image) are
derived from the Android 4.4 version of the program
 ~/android/source/system/core/mkbootimg/mkbootimg.c.

[NOTICE](NOTICE)

I learned about unpacking/repacking ramdisks from [HOWTO: Unpack,
Edit, and Re-Pack Boot
Images](http://android-dls.com/wiki/index.php?title=HOWTO:_Unpack%2C_Edit%2C_and_Re-Pack_Boot_Images)

Use
---

These two programs are intended to be used together when modifying an
existing boot image. I have used these programs to root Nexus factory
images by modifying default.prop in the ramdisk to set ro.secure=0 and
ro.adb.secure=0.

### spbootimg
<pre>
usage: spbootimg
       -i|--input <filename>
</pre>
spbootimg splits an Android boot image file into separate files:

* &lt;bootimg-filename&gt;-kernel - kernel
* &lt;bootimg-filename&gt;-first-ramdisk - ramdisk
* &lt;bootimg-filename&gt;-second-ramdisk - only created if it existed in the input boot image file.
* &lt;bootimg-filename-header&gt; - text file containing constants discovered in the boot image

### pkbootimg
<pre>
usage: pkbootimg
       --kernel <filename>
       --kernel-addr <address>
       --ramdisk <filename>
       --ramdisk-addr <address>
       [ --second <2ndbootloader-filename>]
       [ --cmdline <kernel-commandline> ]
       [ --board <boardname> ]
       [ --pagesize <pagesize> ]
       --second-addr <address>
       --tags-addr <address>
       -o|--output <filename>
</pre>

pkbootimg takes the output of spbootimg: a kernel file, a ramdisk
file, an optional ramdisk file; and using the command line, board,
page size and address information discovered in the original boot
image, packs them into a new boot image which can be flashed onto a
device using fastboot.

pkbootimg is used instead of the 'official' Android mkbootimg since
mkbootimg requires knowledge of the base address used as well as the
fact that, for Nexus devices at least, some boot image files do not
follow the base+offset convention for all addresses. pkbootimg instead
uses the raw addresses detected by spbootimg and is therefore
independent of any choice of base address or inconsistency between the
boot image's addresses and the base + offset address convention.

Note that all of the address parameters including --second-addr are
required even if there was no second ramdisk.

*NOTE|WARNING* You *should* check if spbootimg and pkbootimg work for
your boot image, by re-packing the output of spbootimg and checking if
the new boot image file is *identical* to the original boot image. If
the two boot image files are different, then there is a problem and
you should not use spbootimg and pkbootimg for your boot image file.

### Example Nexus 4 Mako

For example, for a boot image with a single ramdisk such as a Nexus 4 Mako build

<pre>spbootimg -i boot.img</pre>

will for create:

* boot.img-kernel
* boot.img-first-ramdisk
* boot.img-header

where boot.img-header contains:

<pre>Assumptions
kernel_offset = 0x8000
ramdisk_offset = 0x1000000
second_offset = 0xF00000
tags_offset = 0x100
Detected values
kernel_size = 5677280
kernel_addr = 0x80208000
ramdisk_size = 357798
ramdisk_addr = 0x81800000
second_size = 0
second_addr = 0x81100000
tags_addr = 0x80200100
page_size = 2048
name/board = 
cmdline = console=ttyHSL0,115200,n8 androidboot.hardware=mako lpj=67677
Computed values
kernel_base = 0x80200000
ramdisk_base = 0x80800000, kernel_base - ramdisk_offset=0xFFA00000
second_base = 0x80200000, kernel_base - second_offset=0x0
tags_base = 0x80200000, kernel_base - tags_offset = 0x0
WARNING! base addresses do not match!
</pre>

To check if spbootimg and pkbootimg work, re-pack the output of
spbootimg and check if the new boot image file is *identical* to the
original boot image. If it is not, then there is a problem and you
should not use spbootimg and pkbootimg for your boot image file.

<pre>
pkbootimg --kernel boot.img-kernel \
          --kernel-addr 0x80208000 \
          --ramdisk boot.img-first-ramdisk \
          --ramdisk-addr 0x81800000 \
          --cmdline 'console=ttyHSL0,115200,n8 androidboot.hardware=mako lpj=67677' \
          --second-addr 0x81100000 \
          --tags-addr 0x80200100 \
          --output new-boot.img
</pre>

If you wish to modify the ramdisk for example in order to root the
device, you will need to unpack the ramdisk for modifcation:

<pre>
mkdir ramdisk
cd ramdisk
gzip -dc ../boot.img-first-ramdisk | cpio -i
# edit default.prop...

# On Linux you can
find . | cpio -o -H newc | gzip > ../ramdisk-new.gz

# On OSX you can use the mkbootfs program in ~/android/source/out/host/
cd ..
mkbootfs ./ramdisk | gzip > ramdisk-new.gz

pkbootimg --kernel boot.img-kernel \
          --kernel-addr 0x80208000 \
          --ramdisk ramdisk-new.gz \
          --ramdisk-addr 0x81800000 \
          --cmdline 'console=ttyHSL0,115200,n8 androidboot.hardware=mako lpj=67677' \
          --second-addr 0x81100000 \
          --tags-addr 0x80200100 \
          --output new-boot.img
</pre>

### Testing your new boot image

fastboot allows you to boot from a temporary boot image from a kernel and ramdisk using

<pre>
fastboot boot boot.img-kernel ramdisk.img
</pre>

however this may not work if the default values used for the addresses, command line, board etc
do not work for your device.

### Flashing your new boot image.

*WARNING* - you will invalidate your warranty and potentially brick your device.

*USE AT YOUR OWN RISK !*

To permanently flash your new boot image to the device:

<pre>
fastboot flash boot new-boot.img
fastboot reboot
</pre>

Building spbootimg and pkbootimg
--------------------------------

Set up your build environment according to Android's [Downloading and
Building](http://source.android.com/source/building.html)
instructions.

pkbootimg requires the library libmincrypt.a. The easiest (though
slowest) means of obtaining libmincrypt.a is to build Android prior to
attempting to compile spbootimg.c or pkbootimg.c.  It may be possible
to only build the system/core directory though I haven't tested it.

You will need to find the location of your libmincrypt.a directory. On Linux, mine
was located in <code>~/android/source//out/host/linux-x86/obj/STATIC_LIBRARIES/libmincrypt_intermediates/</code>.

Either copy or symlink spbootimg.c and pkbootimg.c into the mkbootimg directory in your
Android source, e.g.  ~/android/source/system/core/mkbootimg/.

Compile and place the binaries in your ~/bin/ directory:

<pre><code>
gcc -m32 -o ~/bin/pkbootimg -I ../include/ pkbootimg.c ~/android/source/out/host/linux-x86/obj/STATIC_LIBRARIES/libmincrypt_intermediates/libmincrypt.a
gcc -m32 -o ~/bin/spbootimg spbootimg.c
</code></pre>

