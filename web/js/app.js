(function() {

var module = angular.module('p1stream', []);

function nullReviver(k, v) {
    if (typeof(v) === 'object' && v !== null && !Array.isArray(v))
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
        });
    });

    source.addEventListener('patch', function(ev) {
        $rootScope.$apply(function() {
            var patch = JSON.parse(ev.data, nullReviver);
            Symmetry.patch(objects, patch, symmetryOptions);
        });
    });
});

module.directive('fixedAspect', function($window, $parse) {
    var doc = $window.document;

    return function(scope, element, attr) {
        var values = attr.fixedAspect.split(':');
        var widthGet = $parse(values[0]);
        var heightGet = $parse(values[1]);
        var aspectEl = doc.createElement('DIV');
        element.addClass('fixed-aspect').append(aspectEl);

        scope.$watch(function(scope) {
            return widthGet(scope) / heightGet(scope);
        }, function(aspect) {
            aspectEl.style.paddingBottom = (100 / aspect) + '%';
        });
    };
});

module.directive('mixerPreview', function($window, $parse) {
    var doc = $window.document;
    var ua = $window.navigator.userAgent;

    return function(scope, element, attr) {
        var el = element[0];

        var childEl;
        scope.$watch(attr.mixerPreview, function(mixerId) {
            if (childEl) {
                el.removeChild(childEl);
                childEl = null;
            }

            if (mixerId) {
                // We unfortunately need this container, because the position based
                // sizing is not enough to correctly size the actual video element.
                childEl = doc.createElement('DIV');
                childEl.className = 'mixer-preview';

                var videoEl;
                if (ua === 'p1stream-mac')
                    videoEl = createWebDocumentView(mixerId);
                else
                    videoEl = createVideo(mixerId);
                videoEl.style.width = '100%';
                videoEl.style.height = '100%';

                childEl.appendChild(videoEl);
                el.appendChild(childEl);
            }
        }, true);
    };

    function createWebDocumentView(mixerId) {
        var el = doc.createElement('OBJECT');
        el.data = 'data:application/x-p1stream-preview,' + mixerId;
        return el;
    }

    function createVideo(mixerId) {
        var el = doc.createElement('VIDEO');
        el.src = '/api/mixers/' + mixerId + '.mkv';
        el.muted = el.defaultMuted = true;
        el.autoplay = true;
        return el;
    }
});

})();
