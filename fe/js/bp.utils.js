/*
 * Copyright 2010 Bobby Powers
 * released under the MIT License
 * <http://www.opensource.org/licenses/mit-license.php>
 *
 * These are various functions that have come in handy.
 */
var bp = {};

bp.utils = function () {
	return {

        LOADING_HTML: "loading...",

        isNull: function (obj) {
		if (typeof obj === 'object' && !obj)
			return true;
		else
			return false;
	},

        isArray: function (obj) {
		// make sure we're a non-null object
		if (typeof obj !== 'object' || !obj)
			return false;

		// from the 'good parts' book, p61
		if (typeof obj.length === 'number' &&
		    !obj.propertyIsEnumerable('length') &&
		    typeof obj.splice === 'function') {

			return true;
		}
		else {
			return false;
		}
	},

	contains: function(arr, obj) {
		var i, n;
		for (i=0, n=arr.length; i<n; ++i)
			if (arr[i] === obj)
				return true;
		return false;
	},
	
        unique: function (arr) {
		var contains = bp.utils.contains;
		var ret = new Array();
		var i, n, j, m;
		for (i=0, n=arr.length; i < n; ++i)
			if (!contains(ret, arr[i]))
				ret.push(arr[i]);
		return ret;
	},

        getNested: function get (obj, name) {
		if (typeof name === 'string') {
			name = name.split('.');
		}
		
		if (name.length === 1 && obj) {
			return obj[name[0]];
		} else {
			return get(obj[name[0]], name.slice(1));
		}
	},

        applyTemplate: function (text, config) {
		var unique = bp.utils.unique,
		getNested = bp.utils.getNested;

		var standins = text.match(/\$\{([\w\.]+)\}/g);
		// unique our variables list, since we'll be doing
		// global find and replaces.
		standins = unique(standins);
		var replacements = standins.map(function (v) {
			// remove the '${' and '}' from the match
			var key = v.slice(2, -1);
			return getNested(config, key);
		});

		var i, re;
		for (i=0; i<standins.length; ++i) {
			// we want to match against '${...}', so we need
			// to escape the stand-in since $, { and } are
			// regex special chars.
			re = new RegExp(standins[i].replace(/\$|\{|\}/g, '\\$&'),
					'g');
			text = text.replace(re, replacements[i]);
		}

		return text;
	}
	};
}();
