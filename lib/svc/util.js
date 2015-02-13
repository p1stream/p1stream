module.exports = function(scope) {
    scope.resolveParam = function(name, type) {
        return function(req, res, next) {
            var id = req.params[name];

            var obj = id && id[0] !== '$' && scope.o[id];
            if (obj && obj.cfg.type === type)
                req.mixer = obj;

            next();
        };
    };
};