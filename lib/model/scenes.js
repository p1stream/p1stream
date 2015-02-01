var _ = require('underscore');

module.exports = function(scope) {
    // Define a scene type, and resolve audio and video sources.
    scope.o.$onCreate('scene', function(obj) {
        obj.$resolveAll('audioSources', function() {
            return _.pluck(obj.cfg.audio, 'sourceId');
        });
        obj.resolveAll('videoSources', function() {
            return _.pluck(obj.cfg.video, 'sourceId');
        });
    });
};
