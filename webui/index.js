var api = require('../api');

module.exports = function(app) {
    app.use(api.staticFiles('/ui', __dirname));
};
