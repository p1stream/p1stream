var _ = require('lodash');

module.exports = function(app) {
    // Define a generic source category.
    app.store.onCreate('source:', function(obj) {
        // Activate when there are consumers or set to prewarm.
        obj.sourceCond = function() {
            return this.numConsumers || this.cfg.warm;
        };
        obj.defaultCond = function() {
            return this.activationCond() && this.sourceCond();
        };
    });

    // Define a video source category.
    app.store.onCreate('source:video:', function(obj) {
        // Track the number of referencing video-mixers.
        obj.watchValue(function() {
            var num = 0;
            for (var from of obj._refs.keys()) {
                if (from.cfg.type === 'video-mixer')
                    num++;
            }
            return num;
        }, function(numConsumers) {
            obj.numConsumers = numConsumers;
            app.mark();
        });
    });

    // Define an audio source category.
    app.store.onCreate('source:audio:', function(obj) {
        // Track the number of referencing audio-mixers.
        obj.watchValue(function() {
            var num = 0;
            for (var from of obj._refs.keys()) {
                if (from.cfg.type === 'audio-mixer')
                    num++;
            }
            return num;
        }, function(numConsumers) {
            obj.numConsumers = numConsumers;
            app.mark();
        });
    });
};
