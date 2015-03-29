var _ = require('lodash');
var ListenerGroup = require('../listenerGroup');

module.exports = function(app) {
    app.store.onCreate('mixer', function(obj) {
        obj._videoHooks = [];
        obj.numFrameListeners = 0;
        obj.resolve('scene');

        // Activate when we have listeners.
        obj.defaultCond = function() {
            return this.activationCond() && this.numFrameListeners;
        };

        // Video mixer activation.
        obj.activation('video mixer object', {
            start: function(lg) {
                obj._videoMixer = app.store.create(_.extend({
                    type: 'video-mixer'
                }, obj.cfg.video));
                obj._videoMixer._ephemeral = true;
                obj._videoMixer.ref(obj);

                // Handle events.
                lg.listen(obj._videoMixer, 'headers', function(headers) {
                    obj._videoHeaders = headers;
                    app.mark();

                    obj.emit('videoHeaders', headers);
                });

                lg.listen(obj._videoMixer, 'frame', function(frame) {
                    obj.emit('videoFrame', frame);
                });

                // Connect sources.
                lg.watchValue(function() {
                    return obj._scene && obj._scene._nativeVideoList;
                }, function(list) {
                    obj._videoMixer.setSources(list);
                });

                // Connect hooks.
                lg.watchValue(function() {
                    return obj._videoHooks;
                }, function(list) {
                    obj._videoMixer.setHooks(list);
                }, 1);
            },
            stop: function() {
                obj._videoMixer.unref(obj);
                obj._videoMixer = null;

                obj._videoHeaders = null;
            }
        });

        // Audio mixer activation.
        obj.activation('audio mixer object', {
            start: function(lg) {
                obj._audioMixer = app.store.create(_.extend({
                    type: 'audio-mixer'
                }, obj.cfg.audio));
                obj._audioMixer._ephemeral = true;
                obj._audioMixer.ref(obj);

                // Handle events.
                lg.listen(obj._audioMixer, 'headers', function(headers) {
                    obj._audioHeaders = headers;
                    app.mark();

                    obj.emit('audioHeaders', headers);
                });

                lg.listen(obj._audioMixer, 'frame', function(frame) {
                    obj.emit('audioFrame', frame);
                });

                // Connect sources.
                lg.watchValue(function() {
                    return obj._scene && obj._scene._nativeAudioList;
                }, function(list) {
                    obj._audioMixer.setSources(list);
                });
            },
            stop: function() {
                obj._audioMixer.unref(obj);
                obj._audioMixer = null;

                obj._audioHeaders = null;
            }
        });

        // Listen for frame events. Takes a map of events to listener
        // functions. Returns a function to cancel listening. Will also
        // increase numFrameListeners and cause the mixer to start running.
        obj.addFrameListener = function(events, options) {
            var isStrong = !(options && options.weak);

            if (isStrong)
                obj.numFrameListeners++;

            var lg = new ListenerGroup(app);
            _.each(events, function(fn, ev) {
                if (typeof(fn) === 'function')
                    lg.listen(obj, ev, fn);
            });

            if (events.videoHook)
                obj._videoHooks.push(events.videoHook);

            app.mark();

            if (options && options.emitInitHeaders) {
                if (events.audioHeaders && obj._audioHeaders)
                    events.audioHeaders(obj._audioHeaders, obj);
                if (events.videoHeaders && obj._videoHeaders)
                    events.videoHeaders(obj._videoHeaders, obj);
            }

            return function() {
                if (!lg)
                    return;

                if (isStrong)
                    obj.numFrameListeners--;

                lg.clearListeners();
                lg = null;

                if (events.videoHook) {
                    var idx = obj._videoHooks.indexOf(events.videoHook);
                    if (idx !== -1)
                        obj._videoHooks.splice(idx, 1);
                }

                app.mark();
            };
        };
    });
};
