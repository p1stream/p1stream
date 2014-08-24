// Destroy the atom-shell commonjs environment.
// This stops scripts from detecting it and changing behavior.
if (typeof(module) !== 'undefined')
    require = exports = module = undefined;
