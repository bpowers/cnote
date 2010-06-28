/*
 * Copyright 2010 Bobby Powers
 * released under the MIT License
 * <http://www.opensource.org/licenses/mit-license.php>
 *
 * Defines the various states of the Deploy project.  Generally
 * each page has both a loading-X and an X state.
 */

// called on document.ready, sets the initial state of the FSM.
$(function () {

bp.fsm.addStates({

    name: 'error',
    enter: function (options) {
        var msg = 'whoops, an error occured. <br />';
        msg += '<pre>' + options.error + '</pre>';

        $(options.content).html(msg);
    }
},

function () {
    var artists = null;
    var loadingHtml = '';

    return {
        name: 'loading-artists',
        enter: function (options) {
            var name = this.name;
            // pop up a loading message.
            $(options.content).html(bp.utils.LOADING_HTML);

            $.ajax({
                url: '/api/artist',
                dataType: 'json',
                success: function (data, status) {
                    artists = data;
                    bp.fsm.transition(name, 'artists');
                }
            });
        },
        exit: function (options) {
	    if (!artists)
		loadingHtml = 'this page intentionally left blank.';
	    else
		artists.map(function (a) {
		    loadingHtml += '<div class="row"><a href="#' + a + '">' +
			a + '</a></div>';
		});
            //loadingHtml = bp.utils.applyTemplate(loadingHtml,
            //                                     options.config);
            $(options.content).html(loadingHtml);
	    $('.row a').bind('click', function (event) {
		event.preventDefault();
		bp.fsm.transition('artists', 'loading-artist',
				  {artist: $(this).text()});
	    });
        }
    };
}(),

{
    name: 'artists',
    enter: function (options) {
        var content = $(options.content);
    },
    exit: function (options) {

    }
},

function () {
    var songs = null;
    var loadingHtml = '';

    return {
        name: 'loading-artist',
        enter: function (options) {
            var name = this.name;
            // pop up a loading message.
            $(options.content).html(bp.utils.LOADING_HTML);

            $.ajax({
                url: '/api/artist/' + options.artist,
                dataType: 'json',
                success: function (data, status) {
                    songs = data;
                    bp.fsm.transition(name, 'artist');
                }
            });
        },
        exit: function (options) {
	    if (!songs)
		loadingHtml = 'this page intentionally left blank.';
	    else
		songs.map(function (song) {
		    loadingHtml += '<div class="row"><a href="/music/' +
			song.path + '">play</a> ' + song.album +
			' - ' + song.title + '</div>';
		});
            //loadingHtml = bp.utils.applyTemplate(loadingHtml,
            //                                     options.config);
            $(options.content).html(loadingHtml);
	    $('.row a').bind('click', function (event) {
		event.preventDefault();
		$('#audio').remove();
		$('#header').append('<audio src="' + $(this).attr('href') +
				    '" id="audio" controls preload></audio>');
		    document.getElementById("audio").muted = false;
	    });
        }
    };
}(),

{
    name: 'artist',
    enter: function (options) {
        var content = $(options.content);
    },
    exit: function (options) {

    }
}


);

    // this config object will be passed to all states as 'options.config'
    // and can be useful for things like very simple templating.
    bp.fsm.setConfig({
        app: {
            name: 'cmusic'
        }
    });

    bp.fsm.transition('', 'loading-artists');
});
