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
    var loadingHtml = '';

    return {
        name: "loading-login",
        enter: function (options) {
            var name = this.name;
            // pop up a loading message.
            $(options.content).html(bp.utils.LOADING_HTML);

            $.ajax({
                url: 'html/login.html',
                dataType: 'html',
                success: function (data, status) {
                    loadingHtml = data;
                    bp.fsm.transition(name, 'login');
                }
            });
        },
        exit: function (options) {
            loadingHtml = bp.utils.applyTemplate(loadingHtml,
                                                 options.config);
            $(options.content).html(loadingHtml);
        }
    };
}(),

{
    name: 'login',
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
            name: "Deploy"
        }
    });

    bp.fsm.transition('', 'loading-login');
});
