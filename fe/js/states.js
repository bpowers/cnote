/*
 * Copyright 2010 Bobby Powers
 * released under the MIT License
 * <http://www.opensource.org/licenses/mit-license.php>
 */
var options = {
    content: '#details',
};

bp.media = function () {

    return {
	playing: function() {
	    $('#selected').attr('id', '');
	    var song = $('#audio')[0].src;
	    $(options.content + ' .row a').filter(function (i, a) {
		if (a.href === song)
		    return true;
		return false;
	    }).parent().attr('id', 'selected');
	},
	canplay: function() {
	    $('#audio')[0].play();
	},
	ended: function() {
	    $('#selected').next().children('a').click();
	}
    }
}();

var infoCb = function(type, clickAction) {
    return function(data, status) {
        var html = '';
        if (!data)
            html = 'no data in response.';
        else
            data.map(function (a) {
                if (!a)
                    return;
                html += '<div class="row"><a href="#' + type+ '=' + a + '">' +
                    decodeURIComponent(a) + '</a></div>';
            });

        $('#' + type).html(html);
        $('#' + type + ' .row a').bind('click', function (event) {
            event.preventDefault();
            clickAction(type, $(this).text());
        });
    }
}

var loadBrowse = function(type) {
    $.ajax({
        url: '/api/' + type,
        dataType: 'json',
        success: infoCb(type, loadDetails)
    });
}

var loadDetails = function(type, target) {
    $.ajax({
        url: '/api/' + type + '/' + encodeURIComponent(target),
        dataType: 'json',
        success: function(data, status) {
	    var html = '';
	    if (!data)
		html = 'no data in response.';
	    else
		data.map(function (song) {
		    if (!song)
			return;
		    html += '<div class="row"><a href="/music/' +
			encodeURIComponent(song.path) + '">play</a> ' +
			decodeURIComponent(song.album) +
			' - ' + decodeURIComponent(song.title) + '</div>';
		});
	    $(options.content).html(html);
	    $(options.content + ' .row a').bind('click', function(event) {
		event.preventDefault();
		$('#now-playing').html('<audio src="' + $(this).attr('href') +
				       '" id="audio" controls preload ' + 
				       'oncanplay="bp.media.canplay()" ' +
				       'onplaying="bp.media.playing()" ' +
				       'onended="bp.media.ended()" ' +
				       '></audio>');
		document.getElementById("audio").muted = false;
	    });
	}
    });
}

// called on document.ready, sets the initial state of the FSM.
$(function () {
    loadBrowse('artist');
    loadBrowse('album');
});
