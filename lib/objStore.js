// Simple object store. Each object has an id, type and config. The entire
// object is synchronized to web clients, only the config part is saved.
//
// Objects can reference eachother, and are garbage collected once all
// references are removed.
//
// Modules can use onCreate to install watchers and add behavior to objects.

function ObjStore() {
    this.map = new Map();
    this.class = Obj;
    this._createListeners = [];
}

module.exports = ObjStore;

// Create an object.
ObjStore.prototype.create = function(cfg, id) {
    if (!cfg.type)
        cfg.type = 'unknown';

    while (!id) {
        id = cfg.type + ':' + Math.random().toString(16).slice(2);
        if (this.map.has(id))
            id = null;
    }

    var obj = new this.class(this, id, cfg);
    this.map.set(obj.id, obj);

    this._createListeners.forEach(function(fn) {
        fn(obj);
    });

    return obj;
};

// Install a callback on create. The type parameter matches on
// `obj.cfg.type`. If type ends with a semicolon, the remainder is a
// wildcard, which allows matching categories and subtypes.
ObjStore.prototype.onCreate = function(type, fn) {
    var self = this;

    var listener;
    if (typeof(type) === 'function') {
        listener = type;
    }
    else {
        listener = function(obj) {
            if (type.slice(-1) === ':' ?
                    obj.cfg.type.slice(0, type.length) === type :
                    obj.cfg.type === type)
                fn(obj);
        };
    }

    self._createListeners.push(listener);
    return function() {
        var idx = self._createListeners.indexOf(listener);
        if (idx !== -1)
            self._createListeners.splice(idx, 1);
    };
};

// Get an object by ID.
ObjStore.prototype.get = function(id) {
    return this.map.get(id);
};

// Garbage collect. Throws away all objects that have no references and aren't
// marked with the sticky flag.
ObjStore.prototype.gc = function() {
    for (var obj of this.map.values()) {
        if (!obj._sticky && obj._refs.size === 0)
            obj.destroy();
    }
};


function Obj(store, id, cfg) {
    this.id = id;
    this.cfg = cfg;
    this._store = store;
    this._refs = new Map();
    this._sticky = false;     // Whether to disable GC.
    this._ephemeral = false;  // Whether to disable saving to config.
}

ObjStore.Obj = Obj;

// Destroy the object.
Obj.prototype.destroy = function() {
    this._store.map.delete(this.id);
};

// Add a reference to this object.
Obj.prototype.ref = function(from) {
    var val = this._refs.get(from) || 0;
    this._refs.set(from, val + 1);
};

// Remove a reference to this object.
Obj.prototype.unref = function(from) {
    var val = this._refs.get(from) || 0;
    if (val === 1)
        this._refs.delete(from);
    else
        this._refs.set(from, val - 1);
};
