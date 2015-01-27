var _ = require('underscore');
var Mixer = require('./mixer');

// Helpers for refs on other objects.
function refHelpers(scope, attr, id) {
    return {
        add: function(obj) {
            var map = obj[attr];
            var current = map[id];
            map[id] = current ? current + 1 : 1;
            scope.$mark();
        },

        remove: function(obj) {
            var map = obj[attr];
            var current = map[id];
            if (current === 1)
                delete map[id];
            else
                map[id] = current - 1;
            scope.$mark();
        },

        resolve: function(type, watchFn, changeFn) {
            var ref = this;

            var cfgProp = type + 'Id';  // 'thingId' in config
            var resProp = '$' + type;  // resolved '$thing'
            var map = scope[type + 's'];  // 'things' index

            scope.$watchValue(watchFn || function() {
                return _.findWhere(map, { $id: scope.cfg[cfgProp] });
            }, function(current, last) {
                if (last) ref.remove(last);
                if (current) ref.add(current);
                scope[resProp] = current;
                if (changeFn) changeFn(current, last);
                scope.$mark();
            });

            scope.$on('$destroy', function() {
                if (scope[resProp])
                    ref.remove(scope[resProp]);
            });
        },

        resolveAll: function(type, watchFn, changeFn) {
            var ref = this;

            var cfgProp = type.slice(0, -1) + 'Ids';  // 'thingIds' in config
            var resProp = '$' + type;  // resolved '$things'
            var map = scope[type];  // 'things' index

            scope[resProp] = [];

            scope.$watchArray(watchFn || function() {
                return _.compact(_.map(scope.cfg[cfgProp], function(id) {
                    return _.findWhere(map, { $id: id });
                }));
            }, function(current, last) {
                _.each(last, ref.remove);
                _.each(current, ref.add);
                scope[resProp] = current;
                if (changeFn) changeFn(current, last);
                scope.$mark();
            });

            scope.$on('$destroy', function() {
                _.each(scope[resProp], ref.remove);
            });
        }
    };
}

module.exports = function(scope) {
    var audioSources, videoSources, scenes, mixers, outputs;

    // Create a model for an audio source.
    scope.createAudioSource = function(cfg) {
        var source = scope.$new();
        source.$id = cfg.id;
        source.cfg = cfg;
        source.$instance = null;
        source.sceneRefs = Object.create(null);  // Filled by scenes.
        source.mixerRefs = Object.create(null);  // Filled by mixers.

        // Track number of active mixers referencing this source.
        source.numConsumers = 0;
        source.$watchValue(function() {
            return _.filter(source.$mixerRefs, function(bool, mixerId) {
                var mixer = _.findWhere(mixers, { $id: mixerId });
                return mixer && mixer.$instance;
            }).length;
        }, function(numConsumers) {
            source.numConsumers = numConsumers;
            source.$mark();
        });

        return source;
    };

    // Create a model for a video source.
    scope.createVideoSource = function(cfg) {
        var source = scope.$new();
        source.$id = cfg.id;
        source.cfg = cfg;
        source.$instance = null;
        source.sceneRefs = Object.create(null);  // Filled by scenes.
        source.mixerRefs = Object.create(null);  // Filled by mixers.

        // Track number of active mixers referencing this source.
        source.numConsumers = 0;
        source.$watchValue(function() {
            return _.filter(source.$mixerRefs, function(bool, mixerId) {
                var mixer = _.findWhere(mixers, { $id: mixerId });
                return mixer && mixer.$instance;
            }).length;
        }, function(numConsumers) {
            source.numConsumers = numConsumers;
            source.$mark();
        });

        return source;
    };

    // Create a model for a scene.
    scope.createScene = function(cfg) {
        var scene = scope.$new();
        scene.$id = cfg.id;
        scene.cfg = cfg;
        scene.$audioSources = [];
        scene.$videoSources = [];
        scene.mixerRefs = Object.create(null);  // Filled by mixers.

        var ref = refHelpers(scene, 'sceneRefs', cfg.id);
        ref.resolveAll('audioSources', function() {
            return _.pluck(cfg.audio, 'sourceId').map(function(id) {
                return _.findWhere(audioSources, { $id: id });
            });
        });
        ref.resolveAll('videoSources', function() {
            return _.pluck(cfg.video, 'sourceId').map(function(id) {
                return _.findWhere(videoSources, { $id: id });
            });
        });

        return scene;
    };

    // Create a model for a mixer.
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
        mixer.outputRefs = Object.create(null);  // Filled by outputs.

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

    // Create a model for an output.
    scope.createOutput = function(cfg) {
        var output = scope.$new();
        output.$id = cfg.id;
        output.cfg = cfg;

        var ref = refHelpers(output, 'outputRefs', cfg.id);
        ref.resolve('mixer');

        return output;
    };

    // Save to config.
    scope.save = function() {
        scope.cfg.audioSources = _.pluck(audioSources, 'cfg');
        scope.cfg.videoSources = _.pluck(videoSources, 'cfg');
        scope.cfg.scenes = _.pluck(scenes, 'cfg');
        scope.cfg.mixers = _.pluck(mixers, 'cfg');
        scope.cfg.outputs = _.pluck(outputs, 'cfg');
        scope.cfg.$save();
        scope.$mark();
    };

    // Load from config.
    audioSources = scope.data.audioSources = _.map(scope.cfg.audioSources, scope.createAudioSource);
    videoSources = scope.data.videoSources = _.map(scope.cfg.videoSources, scope.createVideoSource);
    scenes = scope.data.scenes = _.map(scope.cfg.scenes, scope.createScene);
    mixers = scope.data.mixers = _.map(scope.cfg.mixers, scope.createMixer);
    outputs = scope.data.outputs = _.map(scope.cfg.outputs, scope.createOutput);
};
