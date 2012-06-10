$(function() {

    var Category = Backbone.Model.extend({

	// the API returns a string per album or artist
	parse: function(name) {
	    return {
		name: decodeURIComponent(name)
	    }
	},

	clear: function() {
	    this.destroy();
	}
    });

    var Artist = Category.extend({
	defaults: function() {
	    return {
		name: 'unknown',
		type: 'artist',
		selected: false
	    }
	},
    });

    var Album = Category.extend({
	defaults: function() {
	    return {
		name: 'unknown',
		type: 'album',
		selected: false
	    }
	},
    });

    var escapePath = function(path) {
	path = decodeURIComponent(path);
	var parts = path.split("/");
	var escapedPath = "";
	for (i in parts) {
	    escapedPath += "/" + encodeURIComponent(parts[i]);
	}
	return escapedPath.substring(1);
    }

    var Track = Backbone.Model.extend({
	defaults: function() {
	    return {
		title: 'unknown',
		artist: 'unknown',
		album: 'unknown',
		track: 'unknown',
		path: 'unknown',
		selected: false
	    }
	},

	parse: function(track) {
	    var path = '';
	    return {
		title: decodeURIComponent(track.title),
		artist: decodeURIComponent(track.artist),
		album: decodeURIComponent(track.album),
		track: decodeURIComponent(track.track),
		path: escapePath(track.path)
	    }
	},

	clear: function() {
	    this.destroy();
	}
    });

    var ArtistList = Backbone.Collection.extend({
	url: 'api/artist',
	model: Artist,
	comparator: function(a) {
	    return a.get('name').toLowerCase();
	}
    });

    var AlbumList = Backbone.Collection.extend({
	url: 'api/album',
	model: Album,
	comparator: function(a) {
	    return a.get('name').toLowerCase();
	}
    });

    var TrackList = Backbone.Collection.extend({
	model: Track
    });

    var Artists = new ArtistList;
    var Albums = new AlbumList;
    var Tracks = new TrackList;

    var CategoryView = Backbone.View.extend({
	tagName: 'div',
	template: _.template($('#list-template').html()),

	render: function() {
	    this.$el.html(this.template(this.model.toJSON()));
	    return this;
	}
    });

    var TrackView = Backbone.View.extend({
	tagName: 'div',
	template: _.template($('#track-template').html()),

	events: {
	    'click a.track': 'playSong'
	},

	initialize: function() {
	    this.model.bind('change', this.render, this);
	},

	render: function() {
	    this.$el.html(this.template(this.model.toJSON()));
	    return this;
	},

	playSong: function(e) {
	    e.preventDefault();
	    nowPlaying.start(this.model);
	}
    });

    var NowPlayingView = Backbone.View.extend({
	el: $('#now-playing'),

	events: {
	    'canplay': 'canPlay',
	    'playing': 'playing',
	    'ended': 'ended'
	},

	prev: null,
	curr: null,

	start: function(track) {
	    if (this.curr) {
		this.prev = this.curr;
	    }
	    this.curr = track;
	    this.$el.show();
	    this.el.src = '/music/' + track.get('path');
	},
	canPlay: function() {
	    this.el.play();
	},
	playing: function() {
	    if (this.prev) {
		this.prev.set('selected', false);
		this.prev = null;
	    }
	    this.curr.set('selected', true);
	},
	// current song is over.  see if there is another one to play.
	ended: function() {
	    var i;
	    for (i in Tracks.models) {
		if (this.curr === Tracks.models[i]) {
		    i++;
		    break;
		}
	    }
	    if (i < Tracks.models.length) {
		// found one. play it.
		this.start(Tracks.models[i]);
	    } else {
		// end of the track list. clean up.
		this.curr.set('selected', false);
		this.prev = null;
		this.curr = null;
		this.el.src = '';
	    }
	}
    });

    var nowPlaying = new NowPlayingView;

    var AppView = Backbone.View.extend({

	el: $('#content'),

	events: {
	    'click a.artist': 'loadTracks',
	    'click a.album': 'loadTracks'
	},

	initialize: function() {
	    Artists.bind('add', this.addOne, this);
	    Artists.bind('reset', this.addAllArtists, this);

	    Albums.bind('add', this.addOne, this);
	    Albums.bind('reset', this.addAllAlbums, this);

	    Tracks.bind('add', this.addTrack, this);
	    Tracks.bind('reset', this.addAllTracks, this);

	    Artists.fetch();
	    Albums.fetch();
	},
	addOne: function(a) {
	    var view = new CategoryView({model: a});
	    $('#' + a.get('type')).append(view.render().el);
	},
	addAllArtists: function() {
	    Artists.each(this.addOne);
	},
	addAllAlbums: function() {
	    Albums.each(this.addOne);
	},
	addTrack: function(t) {
	    var view = new TrackView({model: t});
	    $('#tracks').append(view.render().el);
	},
	addAllTracks: function() {
	    $('#tracks').empty();
	    Tracks.each(this.addTrack);
	},
	loadTracks: function(e) {
	    var pieces = e.target.hash.substring(1).split('=');
	    Tracks.url = 'api/' + pieces[0] + '/' + pieces[1];
	    Tracks.fetch();
	}
    });

    var App = new AppView;
})
