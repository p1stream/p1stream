var humanizeDuration = require('humanize-duration');
var ListenerGroup = require('../listenerGroup');

// The default condition.
exports.defaultCond = exports.activationCond = function() {
    return !this.hasError && !this.restarting;
};

// Convenience method to install activation watchers. Takes a parameters
// objects with `cond`, `start` and `stop` functions. `cond` defaults to
// `obj.defaultCond`.
exports.activation = function(name, params) {
    var self = this;
    var activated = false;
    var listenerGroup = new ListenerGroup(this._app);

    this.watchValue(function() {
        return (params.cond || self.defaultCond).call(self);
    }, function(val) {
        if (val && activated === false)
            start();
        else if (!val && activated !== false)
            stop();
    });

    this.on('destroy', function() {
        if (activated !== false)
            stop();
    });

    function start() {
        self._log.info('Starting %s', name);

        activated = Date.now();
        params.start.call(self, listenerGroup);
    }

    function stop() {
        var runtime = humanizeDuration(Date.now() - activated);
        self._log.info('Stopping %s after %s', name, runtime);

        activated = false;
        listenerGroup.clearListeners();
        params.stop.call(self);
    }
};

// Set the restart flag for one full watchable digest. By default, this
// causes all activations to stop and start.
exports.restart = function() {
    var self = this;
    self.restarting = true;
    self._app.mark(function() {
        self.restarting = false;
        self._app.mark();
    });
};

// Clear the error flag, allowing the object to activate again.
exports.clearError = function() {
    this.hasError = false;
    this._app.mark();
};

// Sets the error flag, and logs any arguments given.
exports.fatal = function() {
    if (arguments.length)
        this._log.error.apply(this._log, arguments);
    this.hasError = true;
    this._app.mark();
};
