cnote - a simple, fast web frontend to your music library for Linux
===================================================================

cnote is a way to easily share my (or your!) personal music library
over the world wide web.  It provides a dead-simple web application
enabling anyone to browse your music by artist or album.  Songs are
played using the 'html5' audio tag.  This means that support for
mp3/aac/ogg audio is dependent on your browser.  Firefox doesn't come
with the codecs for mp3 or aac, so unless your music library is all
ogg, use Chrome

I originally designed it so that I could access the music I had on my
desktop at home from my laptop at work, and at this point it performs
the task admirably.

I also wrote it to show that writing web applications in C isn't 'that
hard', and gives you something small and fast.  I stand by that
belief, but frankly I would probably just use Go for any new project
like this.


architecture
------------

cnote compiles to a small native binary, which serves http on port
1969. All the binary does is respond to requests for /artist* and
/album*.  The files in fe/ (frontend) can be served from nginx, along
with your music.  See the config in examples/nginx.conf for how this
works.

There are several paths in src/cnote.c to configure to point cnote at
your library.  When it starts up for the first time, it will crawl
your library and pull information about the songs out of the music
files and into a sqlite database.  It uses inotify to watch your music
library, so rearranging, adding and deleting files will automatically
be reflected in cnote (although for now you will have to reload the
web page).

cnote uses inotify to watch for new/changed files, so it is currently
linux only.  kqueue provides similar functionality on Mac/BSD, so
abstracting this out would be possible, but I don't have plans to do
that right now.


highlights
----------

- A nifty wrapper around inotify, which provides recursive directory
  watching.

- Linux kernel-like linked lists, which use embedded pointers in
  objects, rather than a separately allocated list node object.

- Efficient custom JSON serialization.  A single allocation is
  performed per call to jsonify(), even for complex nested data
  structures (like lists of objects).


status
------

It works excellently for me with the nginx config in examples.  Memory
usage after several hundred thousand requests is stable at about 11
MiB (and half of that is sqlite indexes).  It serves ~750
requests/sec, with sqlite being the limiting factor.


license
-------

cnote is offered under the GPLv3 license, see COPYING for details.
