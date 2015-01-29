var _ = require('underscore');
var refHelpers = require('../refHelpers');
var Mixer = require('../mixer');

module.exports = function(scope) {
    scope.createMixer = function(cfg) {
        var mixer = scope.$new();
        mixer.$id = cfg.id;
        mixer.cfg = cfg;
        mixer.$instance = null;
        mixer.$audioHeaders = null;
        mixer.$videoHeaders = null;
        mixer.$cancelAudioWatcher = null;
        mixer.$cancelVideoWatcher = null;
        mixer.numFrameListeners = 0;
        mixer.hadError = false;
        mixer.$scene = null;
        mixer.$audioSources = [];
        mixer.$videoSources = [];

        var ref = refHelpers(mixer, 'mixerRefs', cfg.id);
        ref.resolve('scene');
        ref.resolveAll('audioSources', function() {
            return mixer.$scene && mixer.$scene.$audioSources;
        });
        ref.resolveAll('videoSources', function() {
            return mixer.$scene && mixer.$scene.$videoSources;
        });

        // Start the mixer.
        function start() {
            var inst = mixer.$instance = new Mixer(mixer.cfg);
            mixer.$mark();

            // Handle events.
            inst.on('audioHeaders', function(headers) {
                mixer.$audioHeaders = headers;
                mixer.$broadcast('audioHeaders', headers);
                mixer.$mark();
            });

            inst.on('audioFrame', function(frame) {
                mixer.$broadcast('audioFrame', frame);
            });

            inst.on('videoHeaders', function(headers) {
                mixer.$videoHeaders = headers;
                mixer.$broadcast('videoHeaders', headers);
                mixer.$mark();
            });

            inst.on('videoFrame', function(frame) {
                mixer.$broadcast('videoFrame', frame);
            });

            inst.on('error', function(err) {
                // FIXME: log error
                mixer.$instance = null;
                mixer.hadError = true;
                mixer.$mark();
            });

            // Connect audio sources.
            mixer.$cancelAudioWatcher = mixer.$watchValue(function() {
                var nativeList = [];
                _.each(mixer.$audioSources, function(source, idx) {
                    if (!source.$instance) return;
                    var el = mixer.$scene.cfg.audio[idx];
                    nativeList.push({
                        source: source.$instance,
                        volume: el.volume || 0
                    });
                });
                return nativeList;
            }, function(list) {
                mixer.$instance.audioMixer.setSources(list);
            }, 2);

            // Connect video sources.
            mixer.$cancelVideoWatcher = mixer.$watchValue(function() {
                var nativeList = [];
                _.each(mixer.$videoSources, function(source, idx) {
                    if (!source.$instance) return;
                    var el = mixer.$scene.cfg.video[idx];
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
                mixer.$instance.videoMixer.setSources(list);
            }, 2);
        }

        // Stop the mixer.
        function stop() {
            mixer.$instance.destroy();
            mixer.$instance = null;

            mixer.$audioHeaders = null;
            mixer.$videoHeaders = null;

            mixer.$cancelAudioWatcher();
            mixer.$cancelAudioWatcher = null;

            mixer.$cancelVideoWatcher();
            mixer.$cancelVideoWatcher = null;

            mixer.$mark();
        }

        // Check if we need to start or stop the mixer.
        mixer.$watchValue(function() {
            return !mixer.hadError && mixer.numFrameListeners;
        }, function(shouldRun) {
            if (shouldRun && !mixer.$instance)
                start();
            else if (!shouldRun && mixer.$instance)
                stop();
        });

        // Stop the mixer on destroy.
        mixer.$on('$destroy', function() {
            if (mixer.$instance)
                stop();
        });

        // Clear the error flag, allowing the mixer to start again.
        mixer.$clearError = function() {
            mixer.hadError = false;
            mixer.$mark();
        };

        // Listen for frame events. Object is a map of events to listener
        // functions. Returns a function to cancel listening. Will also
        // increase numFrameListeners and cause the mixer to start running.
        mixer.$addFrameListener = function(obj, options) {
            mixer.numFrameListeners++;
            mixer.$mark();

            var cancels = _.map(obj, function(fn, ev) {
                return mixer.$on(ev, fn);
            });

            if (options && options.emitInitHeaders) {
                if (obj.audioHeaders && mixer.$audioHeaders)
                    obj.audioHeaders(mixer.$audioHeaders, mixer);
                if (obj.videoHeaders && mixer.$videoHeaders)
                    obj.videoHeaders(mixer.$videoHeaders, mixer);
            }

            return function() {
                if (!cancels) return;

                mixer.numFrameListeners--;
                mixer.$mark();

                _.each(cancels, function(cancel) {
                    cancel();
                });
                cancels = null;
            };
        };

        return mixer;
    };

    scope.data.mixers = _.map(scope.cfg.mixers, scope.createMixer);
    scope.$on('saveConfig', function() {
        scope.cfg.mixers = _.pluck(scope.data.mixers, 'cfg');
        scope.$mark();
    });
};
