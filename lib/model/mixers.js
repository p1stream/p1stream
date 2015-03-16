var _ = require('underscore');
var Mixer = require('../mixer');
var activation = require('../activation');

module.exports = function(scope) {
    scope.o.$onCreate('mixer', function(obj) {
        obj.$instance = null;
        obj.$audioHeaders = null;
        obj.$videoHeaders = null;
        obj.$videoHooks = [];
        obj.$cancelAudioSourcesWatcher = null;
        obj.$cancelVideoSourcesWatcher = null;
        obj.$cancelVideoHooksWatcher = null;
        obj.numFrameListeners = 0;
        obj.hasError = false;

        obj.$resolve('scene');
        obj.$resolve('clock');
        obj.$resolveAll('audioSources', function() {
            return obj.$scene && obj.$scene.$audioSources;
        });
        obj.$resolveAll('videoSources', function() {
            return obj.$scene && obj.$scene.$videoSources;
        });

        // Start the mixer.
        function start() {
            obj.$activeClock = obj.$clock;
            obj.$addRef(obj.$activeClock);

            var inst = obj.$instance = new Mixer(
                scope, obj, obj.cfg, obj.$activeClock.$instance);
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

            inst.on('error', function() {
                obj.hasError = true;
                obj.$mark();
            });

            // Connect audio sources.
            obj.$cancelAudioSourcesWatcher = obj.$watchValue(function() {
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
            obj.$cancelVideoSourcesWatcher = obj.$watchValue(function() {
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

            // Connect video sources.
            obj.$cancelVideoHooksWatcher = obj.$watchValue(function() {
                return obj.$videoHooks;
            }, function(nativeList) {
                inst.videoMixer.setHooks(nativeList);
            }, 1);
        }

        // Stop the mixer.
        function stop() {
            obj.$instance.destroy();
            obj.$instance = null;

            obj.$audioHeaders = null;
            obj.$videoHeaders = null;

            obj.$cancelAudioSourcesWatcher();
            obj.$cancelAudioSourcesWatcher = null;

            obj.$cancelVideoSourcesWatcher();
            obj.$cancelVideoSourcesWatcher = null;

            obj.$cancelVideoHooksWatcher();
            obj.$cancelVideoHooksWatcher = null;

            obj.$removeRef(obj.$activeClock);
            obj.$activeClock = null;

            obj.$mark();
        }

        // Check if we need to start or stop the mixer.
        activation(obj, 'Mixer');
        obj.$activation({
            cond: function() {
                return !obj.hasError && obj.numFrameListeners &&
                    obj.$clock && obj.$clock.$instance;
            },
            start: start,
            stop: stop
        });

        // Listen for frame events. Takes a map of events to listener
        // functions. Returns a function to cancel listening. Will also
        // increase numFrameListeners and cause the mixer to start running.
        obj.$addFrameListener = function(events, options) {
            var isStrong = !(options && options.weak);

            if (isStrong)
                obj.numFrameListeners++;

            var cancels = _.map(events, function(fn, ev) {
                if (ev === 'hook') {
                    obj.$videoHooks.push(fn);
                    return function() {
                        var idx = obj.$videoHooks.indexOf(fn);
                        if (idx !== -1)
                            obj.$videoHooks.splice(idx, 1);
                    };
                }
                else {
                    return obj.$on(ev, fn);
                }
            });

            obj.$mark();

            if (options && options.emitInitHeaders) {
                if (events.audioHeaders && obj.$audioHeaders)
                    events.audioHeaders(obj.$audioHeaders, obj);
                if (events.videoHeaders && obj.$videoHeaders)
                    events.videoHeaders(obj.$videoHeaders, obj);
            }

            return function() {
                if (!cancels)
                    return;

                if (isStrong)
                    obj.numFrameListeners--;

                _.each(cancels, function(cancel) {
                    cancel();
                });
                cancels = null;

                obj.$mark();
            };
        };
    });
};
