(function() {

var module = angular.module('p1stream', []);

module.run(['$rootScope',
function($rootScope) {
    var symmetryOptions = { preserve: true };

    var objects = $rootScope.objects = Object.create(null);
    var source = new EventSource('/api/objects.sse');

    source.addEventListener('reset', function(ev) {
        $rootScope.$apply(function() {
            var val = JSON.parse(ev.data);
            Symmetry.resetObject(objects, val, symmetryOptions);
        });
    });

    source.addEventListener('patch', function(ev) {
        $rootScope.$apply(function() {
            var patch = JSON.parse(ev.data);
            Symmetry.patch(objects, patch, symmetryOptions);
        });
    });
}]);

})();
