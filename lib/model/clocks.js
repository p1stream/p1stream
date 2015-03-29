var native = require('../../build/Release/native.node');

module.exports = function(app) {
    // Define the software clock type.
    app.store.onCreate('clock:p1stream:software-clock', function(obj) {
        obj._instance = null;

        obj.activation('native software clock', {
            start: function() {
                obj._instance = new native.SoftwareClock({
                    numerator: obj.cfg.numerator || 1,
                    denominator: obj.cfg.denominator || 30
                });
                app.mark();
            },
            stop: function() {
                obj._instance.destroy();
                obj._instance = null;
                app.mark();
            }
        });
    });
};
