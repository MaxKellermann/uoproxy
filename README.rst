uoproxy
=======


What is uoproxy?
----------------

uoproxy is a proxy server designed for Ultima Online.  It acts as an
Ultima Online server, and forwards the connection to a 'real' server.

Some of the interesting features:

- transparent auto-reconnect after a server or network failure
  (e.g. DSL disconnect, server maintenance); the UO client won't
  notice, macros keep running without user interaction

- backgrounding the connection, i.e. quit your client and the proxy
  will stay online, for macroing off kill counters

- multi-headed (playing a character with multiple clients)

- character change without logout

- traversing firewalls

- transparent proxying

- faking your client IP - install uoproxy on 10 different servers, and
  have 10 different IP addresses although all clients run on your
  local computer

- faking client version and hardware info

- hide multi client operation

- block spy packets

- circumventing a shard's login server (most freeshards are insecure!)

- easy exploit development (if you know C)


Getting uoproxy
---------------

You can download uoproxy from GitHub:

 https://github.com/MaxKellermann/uoproxy


Installation
------------

uoproxy was developed on Linux, but will probably run on any POSIX
operating system, including Solaris, FreeBSD, MacOS X.  You need the
following to compile it:

- a C++17 compiler (`GCC <https://gcc.gnu.org/>`__ or `clang
  <https://clang.llvm.org/>`__)
- `Meson 1.0 <http://mesonbuild.com/>`__ and `Ninja <https://ninja-build.org/>`__
- `libevent <https://github.com/libevent/libevent/>`__ development
  files (package ``libevent-dev`` or ``libevent-devel``)

Type::

 meson setup output
 ninja -C output install

Then you can start uoproxy on the command line::

 uoproxy -D play.uooutlands.com

Now point your Ultima Online client (encryption disabled) at the
machine running uoproxy.  UOGateway can be used to remove encryption
and to add the uoproxy server to your ``Login.cfg``.

Instead of specifying your shard's login server on the command line,
you can configure it (and other settings) in a configuration file.  It
will be loaded from ``$HOME/.uoproxyrc`` or from
``/etc/uoproxy.conf``.  There is a sample file in
``conf/uoproxy.conf`` in the uoproxy sources.


Configuration
-------------

Most configuration options are only available in the configration
file.  By default, uoproxy looks for it in your home directory
(``~/.uoproxyrc``) and then in the system wide configuration directory
(``/etc/uoproxy.conf``).  The following options are available:

- ``port``: The TCP port <filename>uoproxy</filename> binds on.
  Defaults to ``2593``.

- ``bind``: The IP and the TCP port uoproxy binds on.  You cannot
  specify both ``port`` and ``bind``.

- ``socks4``: Optional SOCKS4 proxy server (e.g. a TOR server).

- ``server``: The login server of the shard you wish to connect to.

- ``server_list``: Emulate a login server.  In the value of this
  option, uoproxy expects a list of game servers (not login servers!)
  in the form ``name=ip:port,name2=ip2:port2,...``.

- ``background``: Keep connections after the last client has exited?
  Defaults to ``no``.

- ``autoreconnect``: Automatically reconnect to game server if the connection
  fails for some reason?  Defaults to ``yes``.

- ``antispy``: Block spy packets?  This includes known packets where
  the client transmits information about the user's
  computer.  Defaults to ``no``.

- ``light``: Block light packets sent by the server, i.e. always
  enable full light level?  Defaults to ``no``.

- ``client_version``: Report this client version to the game server
  and emulate this protocol version.  By default, uoproxy forwards the
  version reported by the first client.

- ``razor_workaround``: Enables the workaround for a Razor bug which
  causes hangs during login.  Defaults to ``no``.

Tips and Tricks
---------------

Multi-Heading
^^^^^^^^^^^^^

uoproxy allows you to play with multiple clients on the one account at
the same time, even if they talk different protocol versions.

Translating between protocol versions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

uoproxy implements several versions of the UO protocol and can
translate between them on-the-fly.  The config option
``client_version`` specifies the protocol version for talkin to the
game server; if that option is not configured, the first client sets
the version.

Now imagine the server does not support a new (or old) protocol
version: just enter a suported version number into the
``client_version`` option, and let uoproxy do the rest.


Troubleshooting
---------------

Dumping packets
^^^^^^^^^^^^^^^

If you experience a problem you cannot solve, and you ask for help,
you should include a packet dump.  uoproxy dumps packets at a
verbosity of 10, i.e. you have to specify 9 ``-v`` options when
starting::

 uoproxy -vvvvvvvvv |tee /tmp/uoproxy.log

Note that this also logs your user name and password.  Since problems
with login packets are rare, you should delete them before you send
the log file.


Credits
-------

Thanks to the people who deciphered the UO network protocol.  Reading
the sources of many free software projects helped a lot during uoproxy
development, namely: RunUO, UOX3, Wolfpack, Iris and others.


Legal
-----

Copyright 2005-2024 Max Kellermann <max.kellermann@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
