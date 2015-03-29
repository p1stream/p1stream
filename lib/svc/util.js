module.exports = function(app) {
    app.resolveParam = function(name, type) {
        return function(req, res, next) {
            var obj = app.o(req.params[name]);
            if (obj && (!type || obj.cfg.type === type))
                req.obj = obj;

            next();
        };
    };
};
