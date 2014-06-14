var httpApp = require('./httpApp');
var staticFiles = require('./staticFiles');

exports = module.exports = function(app) {
    // FIXME
};

exports.createApp = function() {
    var app = httpApp.create();
    exports(app);
    return app;
};

exports.httpApp = httpApp;
exports.staticFiles = staticFiles;
