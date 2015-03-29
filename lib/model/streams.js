module.exports = function(app) {
    // Define a generic stream category.
    app.store.onCreate('stream:', function(obj) {
        // Resolve mixer from config.
        obj.resolve('mixer');

        // Activate when there is a mixer and the active flag is set.
        obj.streamCond = function() {
            return this._mixer && this.active;
        };
        obj.defaultCond = function() {
            return this.activationCond() && this.streamCond();
        };

        // Apply configuration autostart flag.
        obj.active = obj.cfg.autostart;
    });
};
