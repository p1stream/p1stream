(function() {

var module = angular.module('p1stream', []);

module.run(['$rootScope',
function($rootScope) {
    var symmetryOptions = { preserve: true };

    var data = $rootScope.data = {};
    var source = new EventSource('http://localhost:53311/api/data.sse');

    source.addEventListener('reset', function(ev) {
        var val = JSON.parse(ev.data);
        $rootScope.$apply(function() {
            Symmetry.resetObject(data, val, symmetryOptions);
        });
    });

    source.addEventListener('patch', function(ev) {
        $rootScope.$apply(function() {
            var patch = JSON.parse(ev.data);
            Symmetry.patch(data, patch, symmetryOptions);
        });
    });
}]);

})();
