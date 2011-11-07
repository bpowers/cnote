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


Architecture
------------

cnote compiles to a small binary, which serves http on port 1969. A
GET request on / will provide you with a page which uses Javascript to
get information about the artists/albums in your library.

There are several paths in src/cnote.c to configure to point cnote at
your library.  When it starts up for the first time, it will crawl
your library and pull information about the songs out of the music
files and into a sqlite database.  It uses inotify to watch your music
library, so rearranging, adding and deleting files will automatically
be reflected in cnote (although for now you will have to reload the
web page).


status
------

Originally cnote used postgres for its database backend, and not all
the APIs are updated to use the sqlite database (their implementation
is commented out).  Additionally, a small amount of work needs to be
done to use sane-r defaults for the music library location (instead of
hardcoding it for my machine), and allow options to be specified in a
config file.  I do plan on doing this, but wanted to get the code up.
Of course, patches are welcome too :).


license
-------

cnote is offered under the GPLv3 license, see COPYING for details.
I'm open to changing this if that is useful for anyone, let me know.
