var _ = require('underscore');
var refHelpers = require('../refHelpers');

module.exports = function(scope) {
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
                return _.findWhere(scope.data.audioSources, { $id: id });
            });
        });
        ref.resolveAll('videoSources', function() {
            return _.pluck(cfg.video, 'sourceId').map(function(id) {
                return _.findWhere(scope.data.videoSources, { $id: id });
            });
        });

        return scene;
    };

    scope.data.scenes = _.map(scope.cfg.scenes, scope.createScene);
    scope.$on('saveConfig', function() {
        scope.cfg.scenes = _.pluck(scope.data.scenes, 'cfg');
        scope.$mark();
    });
};
