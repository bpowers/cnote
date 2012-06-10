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
	},
	curr: null
    });

    var AlbumList = Backbone.Collection.extend({
	url: 'api/album',
	model: Album,
	comparator: function(a) {
	    return a.get('name').toLowerCase();
	},
	curr: null
    });

    var TrackList = Backbone.Collection.extend({
	model: Track,

	load: function(m) {
	    // clear any existing highlighted artist or album
	    for (k in Collections) {
		if (Collections[k].curr) {
		    Collections[k].curr.set('selected', false);
		    Collections[k].curr = null;
		}
	    }
	    Collections[m.get('type')].curr = m;
	    m.set('selected', true);
	    this.url = 'api/' + m.get('type') + '/' + encodeURIComponent(m.get('name'));
	    this.fetch();
	}
    });

    var Artists = new ArtistList;
    var Albums = new AlbumList;
    var Tracks = new TrackList;
    var Collections = {
	'artist': Artists,
	'album': Albums
    };

    var CategoryView = Backbone.View.extend({
	tagName: 'div',
	template: _.template($('#list-template').html()),

	events: {
	    'click a': 'click'
	},

	initialize: function() {
	    this.model.bind('change', this.render, this);
	},

	curr: null,

	click: function(e) {
	    Tracks.load(this.model);
	},

	render: function() {
	    this.$el.html(this.template(this.model.toJSON()));
	    return this;
	}
    });

    var TrackView = Backbone.View.extend({
	tagName: 'div',
	template: _.template($('#track-template').html()),

	events: {
	    'click a.track': 'play'
	},

	initialize: function() {
	    this.model.bind('change', this.render, this);
	},

	render: function() {
	    this.$el.html(this.template(this.model.toJSON()));
	    return this;
	},

	play: function(e) {
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

	initialize: function() {
	    this.$el.hide();
	},

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
		this.$el.hide();
	    }
	}
    });

    var nowPlaying = new NowPlayingView;

    var AppView = Backbone.View.extend({

	el: $('#content'),

	initial: {},

	initialize: function() {
	    // check to see if we are loading a URL with an
	    // already-selected album or artist
	    var hash = location.hash.substring(1);
	    var parts;
	    if (hash) {
		parts = hash.split('=');
		if (parts.length === 2 && parts[0] in {'artist':1, 'album':1}) {
		    this.initial[parts[0]] = decodeURIComponent(parts[1]);
		}
	    }

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
	    var type = a.get('type');
	    $('#' + type).append(view.render().el);
	    if (type in App.initial && App.initial[type] === a.get('name')) {
		Tracks.load(a);
		App.initial = {};
	    }
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
	}
    });

    var App = new AppView;

    if (navigator.userAgent.indexOf('Chrome') < 0) {
	$('#footer').append('<span style="color: red; font-weight: bold;">(Firefox can\'t play aac or mp3 files.  Use Chrome)</span>');
    }
})
