var _ = require('underscore');

module.exports = function(scope) {
    _.extend(scope.data, {
        audioSources: {},
        videoSources: {},
        scenes: {},
        streams: {}
    });
};
