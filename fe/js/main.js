$(function() {

    var Category = Backbone.Model.extend({

	// the API returns a string per album or artist
	parse: function(name) {
	    return {
		name: name
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
		type: 'artist'
	    }
	},
    });

    var Album = Category.extend({
	defaults: function() {
	    return {
		name: 'unknown',
		type: 'album'
	    }
	},
    });

    var Track = Backbone.Model.extend({
	defaults: function() {
	    return {
		title: 'unknown',
		artist: 'unknown',
		album: 'unknown',
		track: 'unknown',
		path: 'unknown',
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

    var AppView = Backbone.View.extend({
	initialize: function() {
	    Artists.bind('add', this.addOne, this);
	    Artists.bind('reset', this.addAllArtists, this);

	    Albums.bind('add', this.addOne, this);
	    Albums.bind('reset', this.addAllAlbums, this);

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
	}
    });

    var App = new AppView;
})
