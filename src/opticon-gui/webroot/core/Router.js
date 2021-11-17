// ==========================================================================
// Code for handling the hash-route for the application. Page Modules,
// through the Module.Page constructor, will hook themselves into
// the Router structure at creation time.
// ==========================================================================
Router = {
    paths:{},
    active:false,
    current:"/",
    last:null,
    state: {}
}

// --------------------------------------------------------------------------
// Helper class for path elements
// --------------------------------------------------------------------------
Route = function(type) {
    this._type = type?type:"path";
    this._object = {};
}

// --------------------------------------------------------------------------
// Activates the router (it is kept silent when dealing with login).
// --------------------------------------------------------------------------
Router.activate = function(path) {
    var self = Router;
    self.active = true;
    self.handle (path);
}

// --------------------------------------------------------------------------
// Add a new route to the project. A path element named '%' will act
// as a wildcard that will add that element's value to the arguments
// fed into the .activate() method of the Module page.
// --------------------------------------------------------------------------
Router.addRoute = function (path, object) {
    var elements = path.split ("/");
    if (elements[0] == "") elements.splice(0,1);
    var crsr = Router.paths;
    
    for (var i=0; i<elements.length;++i) {
        var e = elements[i];
        var type = (e == "%") ? "wildcard":"path";
        if (! crsr[e]) crsr[e] = new Route (type);
        crsr = crsr[e];
    }
    crsr._object = object;
}

// --------------------------------------------------------------------------
// Resolves an actual path to a Page Module, then calls its .activate()
// --------------------------------------------------------------------------
Router.handle = function (path) {
    var self = Router;
    var elements = path.split ("/");
    if (elements[0] == "") elements.splice(0,1);
    var crsr = self.paths;
    var args = [];
    
    for (var i=0; i<elements.length; ++i) {
        if (crsr[elements[i]]) {
            crsr = crsr[elements[i]];
        }
        else if (crsr['%']) {
            args.push (elements[i]);
            crsr = crsr['%'];
        }
        else return null;
    }
    self.last = self.current;
    self.current = path;
    crsr._object.activate (args);
}

// --------------------------------------------------------------------------
// Convenient method for jumping to a specific route.
// --------------------------------------------------------------------------
Router.go = function (path) {
    window.location.hash = "#" + path;
}

Router.back = function () {
    var self = Router;
    if (self.last) {
        if (self.last != self.current) return self.last;
        return true;
    }
    return false;
}

// --------------------------------------------------------------------------
// Event handler for url hash changes.
// --------------------------------------------------------------------------
window.onhashchange = function() {
    if (Router.active) {
        Router.handle (window.location.hash.split('#')[1]);
    }
}
