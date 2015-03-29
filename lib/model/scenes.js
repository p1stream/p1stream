var _ = require('lodash');

module.exports = function(app) {
    // Define a scene type, and resolve audio and video sources.
    app.store.onCreate('scene', function(obj) {
        // Resolve and build a native list of audio sources.
        obj.resolveAll('audioSources', function() {
            return _.pluck(obj.cfg.audio, 'sourceId');
        });
        obj.watchValue(function() {
            return _.map(obj._audioSources, function(source, idx) {
                var el = obj.cfg.audio[idx];
                return {
                    _obj: source,
                    source: source._instance,
                    volume: el.volume || 0
                };
            });
        }, function(list) {
            obj._nativeAudioList = list;
        }, 2);

        // Resolve and build a native list of video sources.
        obj.resolveAll('videoSources', function() {
            return _.pluck(obj.cfg.video, 'sourceId');
        });
        obj.watchValue(function() {
            return _.map(obj._videoSources, function(source, idx) {
                var el = obj.cfg.video[idx];
                return {
                    _obj: source,
                    source: source._instance,
                    x1: el.x1 || 0,
                    y1: el.y1 || 0,
                    x2: el.x2 || 0,
                    y2: el.y2 || 0,
                    u1: el.u1 || 0,
                    v1: el.v1 || 0,
                    u2: el.u2 || 0,
                    v2: el.v2 || 0
                };
            });
        }, function(list) {
            obj._nativeVideoList = list;
        }, 2);
    });
};
