// Create the application object.

var userPaths = require('./userPaths');
var rootScope = require('./rootScope');
var plugins = require('./plugins');

module.exports = function() {
    // Make sure directories exist.
    userPaths.createWriteableDirsSync();

    // Root model scope.
    var scope = rootScope();

    // Load all services, models and controllers.
    require('./svc/logging')(scope);
    require('./svc/objStore')(scope);
    require('./svc/config')(scope);
    require('./svc/web')(scope);
    require('./svc/util')(scope);

    require('./model/roots')(scope);
    require('./model/sources')(scope);
    require('./model/scenes')(scope);
    require('./model/mixers')(scope);
    require('./model/streams')(scope);

    require('./ctrl/object')(scope);
    require('./ctrl/mixer')(scope);

    // Load all plugins.
    var pluginNames = plugins.pluginNames();
    pluginNames.forEach(function(pluginName) {
        plugins.loadPlugin(pluginName)(scope);
    });

    // Load state from configuration.
    scope.$broadcast('preInit');
    scope.initFromConfig();
    scope.$broadcast('postInit');

    // Expose list of loaded plugins on the root object.
    scope.o['root:p1stream'].pluginNames = pluginNames;

    return scope;
};
