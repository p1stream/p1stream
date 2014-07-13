exports = module.exports = require('./lib/app');

exports.httpApp = require('./lib/httpApp');
exports.staticFiles = require('./lib/staticFiles');
exports.bufferStream = require('./lib/bufferStream');

exports.createApp = function() {
    var app = exports.httpApp.create();
    exports(app);
    return app;
};
