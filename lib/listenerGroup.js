function ListenerGroup(watchable) {
    ListenerGroup.init(this, watchable);
}

module.exports = ListenerGroup;

ListenerGroup.init = function(lg, watchable) {
    lg._list = [];
    lg._watchable = watchable;
};

ListenerGroup.prototype.listen = function(obj, ev, listener) {
    if (!this._list)
        ListenerGroup.init(this);

    obj.addListener(ev, listener);
    this._list.push(function() {
        obj.removeListener(ev, listener);
    });
};

ListenerGroup.prototype.watch = function(fn) {
    this._list.push(this._watchable.watch(fn));
};

ListenerGroup.prototype.watchValue = function(a, b, c) {
    this._list.push(this._watchable.watchValue(a, b, c));
};

ListenerGroup.prototype.clearListeners = function() {
    if (!this._list)
        return;

    var list = this._list;
    this._list = [];

    list.forEach(function(fn) {
        fn();
    });
};
