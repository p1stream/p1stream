var path = require('path');
var api = require('../api');

module.exports = function(app) {
    var docroot = path.join(__dirname, 'web');
    app.use(api.staticFiles('/', docroot));
};
