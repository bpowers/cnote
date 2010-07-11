/*
 * Copyright 2010 Bobby Powers
 * released under the MIT License
 * <http://www.opensource.org/licenses/mit-license.php>
 *
 * This is an implementation of a fininte state machine with an eye
 * towards managing the dynamically loaded html of a page.
 */

/*
 * bp.fsm: Finite State Machine implementation
 */
bp.fsm = function () {
    // javascript equivalent of 'from bp.utils import *'
    var isNull = bp.utils.isNull,
        isArray = bp.utils.isArray;

    // initialize the states with the initial.  we should never get back
    // to this so it doesn't need an enter function.
    var states = {
        '': {
            name:'',
            exit: function () {}
        }
    };
    // this is the _name_ of the current state, not a reference to the
    // state itself.
    var current = '';

    // keep track of where state's content will go.  this allows us to
    // arbitrarily change the location without affecting anything in the
    // states themselves.
    var content = '#details';
    var header = '#browse';

    var config = {};

    return {

        setConfig: function (conf) {
            if (isNull(conf) || typeof conf !== 'object') {
                console.error('invalid param for setConfig');
                return;
            }

            config = conf;
        },

        addState: function (state) {
            if (isNull(state) || typeof state !== 'object') {
                console.error('invalid param for addState');
                return;
            }

            if (states.hasOwnProperty(state.name)) {
                console.warn('overwriting state "%s"', state.name);
            }

            states[state.name] = state;
        },

        // accepts either a sequence of states or an array of states.
        addStates: function () {

            if (arguments.length === 0)
                return;

            var args = arguments;
            if (isArray(arguments[0]))
                args = arguments[0];

            var i;
            for (i=0; i<args.length; ++i)
                this.addState(args[i]);
        },

        // since ajaxy stuff is async, I thought it would be prudent
        // to specify both the from and to states, so we can make
        // sure this transition is still a valid one.
        transition: function (from, to, options) {
            // I'm worried something may happen where we end up with
            // calls to transition coming from non-current states.
            //if (from !== current) {
            //    console.error('expired transition from state "%s"', from);
            //    return;
            //}

            // doesn't make sense to transition from the current state
            // _to_ the current state, so break.
            if (to === current)
                return;

            if (typeof to !== 'string' || !states.hasOwnProperty(to)) {
                console.error('can\'t transition to state "%s"', to);
                return;
            }

            // simplify the state logic by guaranteeing the options object
            // exits and is the correct type
            if (!options || typeof options !== 'object') {
                options = {};
            }

            options.content = content;
            options.header = header;
            options.config = config;

            var currState = states[current];
            var nextState = states[to];

            currState.exit(options);
            current = to;
            nextState.enter(options);
        }
    };
}();
