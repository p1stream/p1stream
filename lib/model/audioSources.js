var _ = require('underscore');

module.exports = function(scope) {
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
                var mixer = _.findWhere(scope.data.mixers, { $id: mixerId });
                return mixer && mixer.$instance;
            }).length;
        }, function(numConsumers) {
            source.numConsumers = numConsumers;
            source.$mark();
        });

        return source;
    };

    scope.data.audioSources = _.map(scope.cfg.audioSources, scope.createAudioSource);
    scope.$on('saveConfig', function() {
        scope.cfg.audioSources = _.pluck(scope.data.audioSources, 'cfg');
        scope.$mark();
    });
};
