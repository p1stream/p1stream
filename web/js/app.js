(function() {

var module = angular.module('p1stream', []);

function nullReviver(k, v) {
    if (typeof(v) === 'object' && v !== null)
        return angular.extend(Object.create(null), v);
    else
        return v;
}

module.run(function($rootScope) {
    var symmetryOptions = { preserve: true };

    var objects = $rootScope.o = Object.create(null);
    var source = new EventSource('/api/objects.sse');

    source.addEventListener('reset', function(ev) {
        $rootScope.$apply(function() {
            var val = JSON.parse(ev.data, nullReviver);
            Symmetry.resetObject(objects, val, symmetryOptions);
            setIds();
        });
    });

    source.addEventListener('patch', function(ev) {
        $rootScope.$apply(function() {
            var patch = JSON.parse(ev.data, nullReviver);
            Symmetry.patch(objects, patch, symmetryOptions);
            setIds();
        });
    });

    function setIds() {
        for (var id in objects)
            objects[id].$id = id;
    }
});

})();
