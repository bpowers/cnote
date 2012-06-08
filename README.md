cnote - a simple, fast web frontend to your music library for Linux
===================================================================

cnote is a way to easily share your personal music library over the
network.  It provides a simple webpage to browse by artist or album
and play songs using the html5 <audio> tag.  This means that support
for mp3/aac/ogg audio is dependent on your browser.  I've personally
had the most success with Chrome.

I originally designed it so that I could access all the music I had on
my desktop at home from my laptop at work, and it serves my needs
quite well.

I also wrote it to show that writing web applications in C isn't 'that
hard'.  I stand by that belief, but, frankly, I would probably just
use Go for any new project like this.


architecture
------------

cnote compiles to a small binary, which serves http on port 1969. All
the binary does is respond to requests for /artist* and /album*.  The
files in fe/ (frontend) can be served from nginx, along with your
music.  See the config in examples/nginx.conf for how this works.

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


status
------

It works excellently for me with the nginx config in examples.  Memory
usage after several hundred thousand requests is stable at 5 MiB.  It
'only' does between 300 and 450 requests/sec.  This response time is
completely dependent on sqlite right now.


license
-------

cnote is offered under the GPLv3 license, see COPYING for details.
