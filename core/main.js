var core = require('./');

var api = require('../api');
var webui = require('../webui');

var app = api.createApp();
webui(app);
app.listen(53311, '127.0.0.1');
