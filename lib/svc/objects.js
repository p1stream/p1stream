var _ = require('lodash');
var util = require('util');
var EventEmitter = require('events');
var ObjStore = require('../objStore');
var ListenerGroup = require('../listenerGroup');

var BaseObj = ObjStore.Obj;
var BaseObjProto = BaseObj.prototype;

module.exports = function(app) {
    app.store = new ObjStore();

    // Extend the object prototype.
    function Obj(store, id, cfg) {
        BaseObj.call(this, store, id, cfg);
        EventEmitter.init.call(this);
        ListenerGroup.init(this, app);
        this._app = app;
        this._log = app.log.child({ obj: id });
        this._log.trace("Object created");
        app.mark();
    }
    util.inherits(Obj, BaseObj);

    _.extend(Obj.prototype, EventEmitter.prototype);
    _.extend(Obj.prototype, ListenerGroup.prototype);

    _.extend(Obj.prototype, require('../obj/resolve'));
    _.extend(Obj.prototype, require('../obj/activation'));
    _.extend(Obj.prototype, require('../obj/nativeHelpers'));

    Obj.prototype.destroy = function() {
        BaseObjProto.destroy.call(this);
        this._log.trace("Object destroyed");
        this.emit('destroy');
        app.mark();
    };

    Obj.prototype.ref = function(from) {
        BaseObjProto.ref.call(this, from);
        this._log.trace('ref from %s', from.id);
        app.mark();
    };

    Obj.prototype.unref = function(from) {
        BaseObjProto.unref.call(this, from);
        this._log.trace('unref from %s', from.id);
        app.mark();
    };

    app.store.class = Obj;

    // Garbage collect after digest.
    app.on('postDigest', function() {
        app.store.gc();
    });

    // Convenience getter function.
    app.o = function(id) {
        return app.store.get(id);
    };
};
