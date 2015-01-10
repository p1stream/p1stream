// Destroy the atom-shell commonjs environment.
// This stops scripts from detecting it and changing behavior.
if (typeof module !== 'undefined') {
    // jshint -W020
    require = exports = module = undefined;
    // jshint +W020
}
