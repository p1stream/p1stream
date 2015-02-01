var _ = require('underscore');
var Mixer = require('../mixer');

module.exports = function(scope) {
    scope.o.$onCreate('mixer', function(obj) {
        obj.$instance = null;
        obj.$audioHeaders = null;
        obj.$videoHeaders = null;
        obj.$cancelAudioWatcher = null;
        obj.$cancelVideoWatcher = null;
        obj.numFrameListeners = 0;
        obj.hadError = false;

        obj.$resolve('scene');
        obj.$resolveAll('audioSources', function() {
            return obj.$scene && obj.$scene.$audioSources;
        });
        obj.$resolveAll('videoSources', function() {
            return obj.$scene && obj.$scene.$videoSources;
        });

        // Start the mixer.
        function start() {
            var inst = obj.$instance = new Mixer(obj.cfg);
            obj.$mark();

            // Handle events.
            inst.on('audioHeaders', function(headers) {
                obj.$audioHeaders = headers;
                obj.$broadcast('audioHeaders', headers);
                obj.$mark();
            });

            inst.on('audioFrame', function(frame) {
                obj.$broadcast('audioFrame', frame);
            });

            inst.on('videoHeaders', function(headers) {
                obj.$videoHeaders = headers;
                obj.$broadcast('videoHeaders', headers);
                obj.$mark();
            });

            inst.on('videoFrame', function(frame) {
                obj.$broadcast('videoFrame', frame);
            });

            inst.on('error', function(err) {
                // FIXME: log error
                obj.$instance = null;
                obj.hadError = true;
                obj.$mark();
            });

            // Connect audio sources.
            obj.$cancelAudioWatcher = obj.$watchValue(function() {
                var nativeList = [];
                _.each(obj.$audioSources, function(source, idx) {
                    if (!source.$instance) return;
                    var el = obj.$scene.cfg.audio[idx];
                    nativeList.push({
                        source: source.$instance,
                        volume: el.volume || 0
                    });
                });
                return nativeList;
            }, function(list) {
                inst.audioMixer.setSources(list);
            }, 2);

            // Connect video sources.
            obj.$cancelVideoWatcher = obj.$watchValue(function() {
                var nativeList = [];
                _.each(obj.$videoSources, function(source, idx) {
                    if (!source.$instance) return;
                    var el = obj.$scene.cfg.video[idx];
                    nativeList.push({
                        source: source.$instance,
                        x1: el.x1 || 0,
                        y1: el.y1 || 0,
                        x2: el.x2 || 0,
                        y2: el.y2 || 0,
                        u1: el.u1 || 0,
                        v1: el.v1 || 0,
                        u2: el.u2 || 0,
                        v2: el.v2 || 0
                    });
                });
                return nativeList;
            }, function(list) {
                inst.videoMixer.setSources(list);
            }, 2);
        }

        // Stop the mixer.
        function stop() {
            obj.$instance.destroy();
            obj.$instance = null;

            obj.$audioHeaders = null;
            obj.$videoHeaders = null;

            obj.$cancelAudioWatcher();
            obj.$cancelAudioWatcher = null;

            obj.$cancelVideoWatcher();
            obj.$cancelVideoWatcher = null;

            obj.$mark();
        }

        // Check if we need to start or stop the mixer.
        obj.$watchValue(function() {
            return !obj.hadError && obj.numFrameListeners;
        }, function(shouldRun) {
            if (shouldRun && !obj.$instance)
                start();
            else if (!shouldRun && obj.$instance)
                stop();
        });

        // Stop the mixer on destroy.
        obj.$on('$destroy', function() {
            if (obj.$instance)
                stop();
        });

        // Clear the error flag, allowing the mixer to start again.
        obj.$clearError = function() {
            obj.hadError = false;
            obj.$mark();
        };

        // Listen for frame events. Takes a map of events to listener
        // functions. Returns a function to cancel listening. Will also
        // increase numFrameListeners and cause the mixer to start running.
        obj.$addFrameListener = function(events, options) {
            obj.numFrameListeners++;
            obj.$mark();

            var cancels = _.map(events, function(fn, ev) {
                return obj.$on(ev, fn);
            });

            if (options && options.emitInitHeaders) {
                if (events.audioHeaders && obj.$audioHeaders)
                    events.audioHeaders(obj.$audioHeaders, obj);
                if (events.videoHeaders && obj.$videoHeaders)
                    events.videoHeaders(obj.$videoHeaders, obj);
            }

            return function() {
                if (!cancels) return;

                obj.numFrameListeners--;
                obj.$mark();

                _.each(cancels, function(cancel) {
                    cancel();
                });
                cancels = null;
            };
        };
    });
};
