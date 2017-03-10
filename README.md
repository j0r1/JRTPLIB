JRTPLIB
=======

> Developed at the [Expertise Centre for Digital Media (EDM)](http://www.edm.uhasselt.be),
> a research institute of the [Hasselt University](http://www.uhasselt.be)

Library location and contact
----------------------------

Normally, you should be able to download the latest version of the library
from this url: [http://research.edm.uhasselt.be/jori/jrtplib](http://research.edm.uhasselt.be/jori/jrtplib)

Documentation can be found at [jrtplib.readthedocs.io](http://jrtplib.readthedocs.io)

If you have questions about the library, you can mail me at:
[jori.liesenborgs@gmail.com](mailto:jori.liesenborgs@gmail.com)

Acknowledgment
--------------

I would like thank the people at the Expertise Centre for Digital Media
for giving me the opportunity to create this rewrite of the library.
Special thanks go to Wim Lamotte for getting me started on the RTP
journey many years ago.

Disclaimer & Copyright
----------------------

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
    OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.

Installation notes
------------------

* Use the [CMake](https://cmake.org/) build system to compile the library. 
  In case extra include directories or libraries are needed, you can use the 
  `ADDITIONAL_` CMake variables to specify these. They will be stored in both 
  the resulting JRTPLIB CMake configuration file and the pkg-config file.

* The library documentation can be generated using Doxygen. An on-line
  version can be found at [http://jrtplib.readthedocs.io](http://jrtplib.readthedocs.io)

* For systems with low memory or for applications which will involve only
  a few participants at a time:  
  You can set the `HASHSIZE` defines in `rtpsources.h`, `rtpudpv4transmitter.h`
  and `rtpudpv6transmitter.h` to a lower value to avoid memory being wasted.
  Note that the library will have to be recompiled.

* Used defines:

    - `WIN32`: For compilation on an Win32 platform.

    - `_WIN32_WCE`: Define needed for compilation on a WinCE platform

    - `RTP_HAVE_SYS_FILIO`:  Set if `<sys/filio.h>` exists.

    - `RTP_HAVE_SYS_SOCKIO`: Set if `<sys/sockio.h>` exists.

    - `RTP_BIG_ENDIAN`: If set, assume big-endian byte ordering.

    - `RTP_SOCKLENTYPE_UINT`: Indicates that getsockname used an unsigned int 
      as its third parameter.

    - `RTP_HAVE_SOCKADDR_LEN`: Indicates that struct sockaddr has an sa_len 
      field.

    - `RTP_SUPPORT_IPV4MULTICAST`: Enables support for IPv4 multicasting.

    - `RTP_SUPPORT_THREAD`: Enables support for JThread.

    - `RTP_SUPPORT_SDESPRIV`: Enables support for RTCP SDES private items.

    - `RTP_SUPPORT_PROBATION`: If set, a few consecutive RTP packets are 
      needed to validate a member.

    - `RTP_SUPPORT_GETLOGINR`: If set, the library will use getlogin_r instead
      of getlogin.

    - `RTP_SUPPORT_IPV6`: If set, IPv6 support is enabled.

    - `RTP_SUPPORT_IPV6MULTICAST`: If set, IPv6 multicasting support is enabled.

    - `RTP_SUPPORT_SENDAPP`: If set, sending of RTCP app packets is enabled.

    - `RTP_SUPPORT_MEMORYMANAGEMENT`: If set, the memory management system is 
      enabled.

    - `RTP_SUPPORT_RTCPUNKNOWN`: If set, sending of unknown RTCP packets is 
      enabled.

    - `RTPDEBUG`: Enables some memory tracking functions and some debug 
      routines.
    
Cross-compilation of JThread & JRTPLIB for Android
--------------------------------------------------

**Warning:** When cross-compiling, the configuration defaults to big-endian.
But since most Android systems are little-endian, you should probably change
this setting in the CMake configuration.

The approach I follow for cross-compiling these libraries for the Android
platform is sketched below. The following lines are stored in a file called 
`toolchain.cmake` (for example):

    set(CMAKE_SYSTEM_NAME Android)
    set(CMAKE_SYSTEM_VERSION 21) # API level
    set(CMAKE_ANDROID_ARCH_ABI armeabi-v7a)
    set(CMAKE_ANDROID_NDK /path/to/ndk-bundle/)
    set(CMAKE_ANDROID_STL_TYPE gnustl_static)
    
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
    
When starting CMake, first for JThread and afterwards for JRTPLIB, I then manually 
add the following entries:

    CMAKE_TOOLCHAIN_FILE /path/to/toolchain.cmake
    CMAKE_INSTALL_PREFIX /path/to/installation/directory
    CMAKE_FIND_ROOT_PATH /path/to/installation/directory

For example, I like to use the `ccmake` program, which would yield
the following command line:

    ccmake -DCMAKE_TOOLCHAIN_FILE=/path/to/toolchain.cmake \
           -DCMAKE_INSTALL_PREFIX=/path/to/installation/directory \
           -DCMAKE_FIND_ROOT_PATH=/path/to/installation/directory \
           /path/to/main/CMakeLists.txt

After configuring JThread this way, just build and install it. The same CMake
procedure for JRTPLIB should then automatically detect the correct JThread
(so the one that's installed in your cross-compilation installation directory),
after which you can again build and install the RTP library.
