var _ = require('underscore');

module.exports = function(scope) {
    // Define a root category. These are always sticky and usually singleton,
    // meaning each subtype only has one instance, where the ID is the same as
    // the type name.
    //
    // Each plugin should create a root to store its settings, reference its
    // other objects and expose any other useful 'global' runtime data to web
    // clients.
    scope.o.$onCreate('root:', function(obj) {
        obj.$sticky = true;
    });

    // The P1stream root. This references scenes and mixers. The order of the
    // list here is also the UI order.
    scope.o.$onCreate('root:p1stream', function(obj) {
        obj.$resolveAll('scenes');
        obj.$resolveAll('mixers');
    });

    // Set config defaults for `root:p1stream` before init.
    scope.$on('preInit', function() {
        var settings = scope.cfg['root:p1stream'] ||
            (scope.cfg['root:p1stream'] = {});
        _.defaults(settings, {
            type: 'root:p1stream',
            sceneIds: [],
            mixerIds: []
        });
    });
};
