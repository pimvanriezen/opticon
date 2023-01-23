// ==========================================================================
// Code for handling external javascript modules, dynamically loaded.
// ==========================================================================
Module = {
    count:0,
    loaded:0
};

// --------------------------------------------------------------------------
// End of callback-chain, called when all parts of a module have beenn
// loaded and initialized. Calls the module object's .create() function,
// and triggers the module loader's callback if this is the last module
// to finish.
// --------------------------------------------------------------------------
Module.done = function(mname) {
    var self = Module;
    var exec = mname + '.create()';
    console.vueContext = mname;
    console.log ("[Module] Calling " + exec);
    try {
        eval (exec);
    }
    catch (e) {
        console.log ("got exception: ", e);
        throw (e);
    }
    self.loaded++;
    $("#loadprogress")[0].setAttribute("value",self.loaded);
    if (self.loaded == self.count) {
        $("#progress").hide();
        self.whendone();
    }
}

// --------------------------------------------------------------------------
// Load javascript code for a module (after markup has been handled)
// --------------------------------------------------------------------------
Module.loadCode = function(mname, mtype) {
    var self = Module;
    var jsurl = "module/" + mname + "." + mtype + ".js";
    console.log ("[Module] Loading code: " + jsurl);

    jsurl = jsurl + '?' + Math.random();

    $.ajax ({url: jsurl, dataType:'text'}).done (function(src) {
        try {
            eval (src);
        }
        catch (e) {
            let msg = jsurl.split('?')[0] + ": " + e.message;
            setTimeout (console.error.bind (console,msg));
            return;
            throw (e);
        }
            
        self.done(mname);
    }).fail (function (jqx, textstatus) {
        console.log ("ajax fail");
    });
}

// --------------------------------------------------------------------------
// Load API code for an API module.
// --------------------------------------------------------------------------
Module.loadAPI = function(mname) {
    var self = Module;
    var jsurl = "module/" + mname + ".api.js";
    console.log ("[Module] Loading API: " + jsurl);

    jsurl = jsurl + '?' + Math.random();

    $.ajax ({url: jsurl}).done (function(src) {
        try {
            eval (src);
        }
        catch (e) {
            let msg = jsurl.split('?')[0] + ": " + e.message;
            setTimeout (console.error.bind (console,msg));
            return;
            throw (e);
        }
            
        self.done("API." + mname);
    }).fail (function (jqx, textstatus) {
        console.log ("ajax fail");
    });
}

// --------------------------------------------------------------------------
// Decorates freshly loaded HTML for the module, then jumps to
// loading the javascript part.
// --------------------------------------------------------------------------
Module.decorateMarkup = function(mname, mtype) {
    var self = Module;
    Markup.decorate (mname);
    Vidi.renderComponents (document.getElementById (mname));
    self.loadCode (mname, mtype);
}

// --------------------------------------------------------------------------
// Loads the markup HTML for a module, then jumps to the decorators.
// --------------------------------------------------------------------------
Module.loadMarkup = function(mname, mtype) {
    var self = Module;
    var htmlurl = "module/" + mname + "." + mtype + ".html";
    console.log ("[Module] Loading markup: " + htmlurl);
    
    htmlurl = htmlurl + '?' + Math.random();
    
    elm = $("<div/>").load (htmlurl,function(){
        self.decorateMarkup(mname, mtype)
    })[0];
    
    elm.style.display = "none";
    elm.id = mname;
    if (mtype == "page") elm.className = "uContent";
    else if (mtype == "wizard") elm.className = "uWizard";
    else if (mtype == "dialog") elm.className = "uDialog";
    
    $("#content").append (elm);
}

// --------------------------------------------------------------------------
// Goes over all <u-module> tags in the document and loads the indicated
// modules.
// --------------------------------------------------------------------------
Module.init = function(whendone) {
    var self = Module;
    
    qModule = $("u-module");
    self.count = qModule.length;
    $("#loadprogress")[0].setAttribute("max",self.count);
    $("#loadprogress")[0].setAttribute("value", 0);
    self.whendone = function() {
        qModule.remove();
        whendone();
    }

    Vidi.renderComponents (document);
    Markup.decorate ("main");
    
    for (var i=0; i<self.count; ++i) {
        var mname = qModule[i].getAttribute('name');
        var mtype = qModule[i].getAttribute('type');
        if (mtype == "api") {
            self.loadAPI(mname);
        }
        else {
            self.loadMarkup (mname, mtype);
        }
    }
}

// --------------------------------------------------------------------------
// Variables keeping track of per-module background functions.
// --------------------------------------------------------------------------
Module.backgroundInterval = 0;
Module.backgroundRef = null;
Module.refresh = function() {}

// --------------------------------------------------------------------------
// Background task, calls the refresh and sets up the timer again.
// --------------------------------------------------------------------------
Module.backgroundTask = function() {
    var self = Module;
    self.refresh();
    self.backgroundRef = setTimeout (self.backgroundTask,
                                     self.backgroundInterval);
}

// --------------------------------------------------------------------------
// Sets the background task for a module. Cancels any previous one.
// --------------------------------------------------------------------------
Module.setBackground = function (f) {
    var self = Module;
    if (self.backgroundRef) clearTimeout (self.backgroundRef);
    self.refresh = f;
    if (self.backgroundInterval == 0) return;
    self.backgroundRef = setTimeout (self.backgroundTask,
                                     self.backgroundInterval);
}

// --------------------------------------------------------------------------
// Disables the running background task.
// --------------------------------------------------------------------------
Module.disableBackground = function () {
    var self = Module;
    if (self.backgroundRef) {
        clearTimeout (self.backgroundRef);
        self.backgroundRef = null;
    }
}

// --------------------------------------------------------------------------
// Constructor for the API module class.
// --------------------------------------------------------------------------
Module.API = function(id) {
    this.id = id;
    this.generation = {};
}

// --------------------------------------------------------------------------
// API utility function. Takes what assumes are functions that will
// all update the same data, but it waits for 5 seconds and only
// calls the last-submitted function.
// --------------------------------------------------------------------------
Module.API.prototype.delayedSubmit = function (id, cb) {
    // Make the id property optional
    if (! cb) {
        cb = id;
        id = "main";
    }
    if (! this.generation[id]) this.generation[id] = 0;
    this.generation[id]++;
    var gen = this.generation[id];
    var that = this;
    setTimeout (function() {
            if (that.generation[id] == gen) cb();
        }, 2500);
}

// --------------------------------------------------------------------------
// Constructor for the Page module class. Also adds the ojbect to the router.
// --------------------------------------------------------------------------
Module.Page = function(id,route,navigationlevel,alias) {
    this.id = id;
    if (route) Router.addRoute (route, this);
    if (navigationlevel) this.navigationlevel = navigationlevel;
    else this.navigationlevel = 0;
    this.root = route.split('/')[1];
    if (alias) Router.addRoute (alias, this);
    this.listenpaths = [];
    this.listeners = [];
}

// --------------------------------------------------------------------------
// Global context variables to keep track of where a new page is in relation
// to a previous one.
// --------------------------------------------------------------------------
Module.Page.lastlevel = 1; // Last page's navigation level.
Module.Page.lastroot = ""; // Last page's root directory
Module.Page.current = {}; // Object of the currently active page.

// --------------------------------------------------------------------------
// Sets the listenpath for change events that will cause the module to
// refresh. If the module is already listening, the listener is replaced,
// oterhwise none is created yet, instead this is left to show().
// --------------------------------------------------------------------------
Module.Page.prototype.setListenPath = function(path) {
    if (this.listeners.length) {
        for (var i in this.listeners) {
            EventListener.unsubscribe (this.listeners[i]);
        }
    }
    this.listeners = [EventListener.subscribe (path, this.refresh)];
    this.listenpaths = [path];
}

// --------------------------------------------------------------------------
// Adds an extra listen path to the page.
// --------------------------------------------------------------------------
Module.Page.prototype.addListenPath = function(path) {
    this.listenpaths.push (path);
    if (this.listeners.length) {
        this.listeners.push (EventListener.subscribe (path, this.refresh));
    }
    else {
        this.listeners = [EventListener.subscribe (path, this.refresh)];
    }
}

// --------------------------------------------------------------------------
// Callback for requires, makes it easy to disable a button if the user
// doesn't have access to a role. Since vue is being a little bitch about
// giving you access to anything in the global context, it has to be in
// the module object, so it gets copies to the View.
// --------------------------------------------------------------------------
Module.Page.prototype.haveRole = function(role) {
    let res = App.haveRole (role);
    return res;
}

// --------------------------------------------------------------------------
// Virtual .activate() call. Just whines, needs to be defined at the
// module level.
// --------------------------------------------------------------------------
Module.Page.prototype.activate = function() {
    console.error ("Unoverloaded .activate() in module " + this.id);
}

// --------------------------------------------------------------------------
// Virtual .create() call. Same deal as .activate().
// --------------------------------------------------------------------------
Module.Page.prototype.create = function() {
    console.error ("Unoverloaded .create() in module " + this.id);
}

// --------------------------------------------------------------------------
// Stand-in empty background function.
// --------------------------------------------------------------------------
Module.Page.prototype.refresh = function() {
}

// --------------------------------------------------------------------------
// Makes the page visibble.
// --------------------------------------------------------------------------
Module.Page.prototype.show = function() {
    if (this.listenpaths.length) {
        if (this.listeners) {
            for (var i in this.listeners) {
                EventListener.unsubscribe (this.listeners[i]);
            }
        }
        this.listeners = [];
        for (var pi in this.listenpaths) {
            var path = this.listenpaths[pi];
            this.listeners.push (EventListener.subscribe (path, this.refresh));
        }
    }

    if (Module.Page.current &&
        Module.Page.current != this && Module.Page.current.hide) {
        Module.Page.current.hide();
    }

    Module.Page.current = this;
    if (this.backgroundInterval) Module.setBackground (this.refresh);
    $(".uContent input").blur();
    $(".uContent:visible").hide();
    $("#"+this.id+" li.selected").removeClass("selected");
    
    if (App && (! App.animateNavigation)) {
        $(this.View.$el).show();
    }
    else {
        if (Module.Page.lastroot && this.root != Module.Page.lastroot) {
            // Don't do animations when switching between roots.
            $(this.View.$el).show();
        }
        else if (this.navigationlevel > Module.Page.lastlevel) {
            // if we're going deeper, slide in.
            let el = this.View.$el;
            let q = $(el).find(".uSearch,.uSelectList,.uSearchResults,"+
                               ".uEditPane:not(.letterbox)");
            let w = $(el).outerWidth();
            q.css("marginRight",-w).css("paddingLeft",w);
            $(el).show();
            q.animate ({
                paddingLeft:0,
                marginRight:0
            },$.speed(180));
        }
        else if (this.navigationlevel < Module.Page.lastlevel) {
            // if we're going shallower, fade in
            $(this.View.$el).show(); // fadeIn(100);
        }
        else {
            // otherwise, forget about animation and just show.
            $(this.View.$el).show();
        }
    }
    Module.Page.lastlevel = this.navigationlevel;
    Module.Page.lastroot = this.root;
}

// --------------------------------------------------------------------------
// Hides the page from view.
// --------------------------------------------------------------------------
Module.Page.prototype.hide = function() {
    if (this.listeners.length) {
        for (var i in this.listeners) {
            EventListener.unsubscribe (this.listeners[i]);
        }
    }
    this.listeners = [];
    if (this.onHide) this.onHide();
    $(this.View.$el).hide();
}

// --------------------------------------------------------------------------
// Creates a Vue instance under .View, with all non-prototyped methods
// also mixed in as Vue methods.
// --------------------------------------------------------------------------
Module.Page.prototype.createView = function(data) {
    let self = this;
    
    var opt = {
        el: '#'+this.id,
        model: data,
        methods:{},
    };
    
    for (var k in this) {
        if (typeof this[k] == 'function') {
            if (k == "haveRole" || Module.Page.prototype[k] == undefined) {
                if (k != 'create') opt.methods[k] = this[k];
            }
        }
    }
    
    opt.updated = function() {
        if (self.onVueUpdate) self.onVueUpdate();
        MarkupDecorators.button.mountBadges($("#"+self.id))
    }
    
    self.Vidi = new Vidi.View (this.id, opt);
    self.View = this.Vidi.view;
    console.log ("View created",self);
    MarkupDecorators.button.mountBadges($("#"+this.id))
}

// --------------------------------------------------------------------------
// Constructor for the Dialog module class.
// --------------------------------------------------------------------------
Module.Dialog = function(id) {
    this.id = id;
}

// --------------------------------------------------------------------------
// Fades in the dialog, and shows the overlay to prevent non-modal action.
// --------------------------------------------------------------------------
Module.Dialog.prototype.show = function() {
    var self = this;
    Module.disableBackground();
    $("input:visible").blur();
    $("#overlay").fadeIn();
    $(this.View.$el).fadeIn();
    setTimeout (function() {
        if (document.activeElement.className == 'uDoc') {
            $(self.View.$el).find("input").first().focus();
        }
    }, 400);
}

// --------------------------------------------------------------------------
// Fade out the dialog and hide the overlay.
// --------------------------------------------------------------------------
Module.Dialog.prototype.hide = function() {
    $("#overlay").fadeOut();
    $("input").blur();
    var elm = (this.View == undefined) ? this.$el : this.View.$el;
    $(elm).fadeOut();
}

// --------------------------------------------------------------------------
// Perform full validation on all the elemetns in the view.
// --------------------------------------------------------------------------
Module.Dialog.prototype.validateAll = function() {
    var v = this;
    if (v.view == undefined) v = this.View;
    else v = v.view;
    if (! v.validate) return true;
    for (k in v) {
        if (typeof (v[k]) == "function") continue;
        if (! v.validate (k, v[k])) return false;
    }
    return true;
}

// --------------------------------------------------------------------------
// Sets up Vue.
// --------------------------------------------------------------------------
Module.Dialog.prototype.createView = function(data) {
    let self = this;
    var opt = {
        el: '#'+this.id,
        model: data,
        methods:{}
    };
    
    for (var k in this) {
        if (typeof this[k] == 'function') {
            if (k != 'create' && k !='hide') opt.methods[k] = this[k];
        }
    }
    
    self.Vidi = new Vidi.View (self.id, opt);
    self.View = this.Vidi.view;
}

// --------------------------------------------------------------------------
// Constructor for the Wizatd module class.
// --------------------------------------------------------------------------
Module.Wizard = function(id,pagecount) {
    this.id = id;
    this.pagecount = pagecount;
}

// --------------------------------------------------------------------------
// Fades in the dialog, and shows the overlay to prevent non-modal action.
// --------------------------------------------------------------------------
Module.Wizard.prototype.show = function() {
    var self = this;
    Module.disableBackground();
    $("input:visible").blur();
    this.View.page = 1;
    $("#overlay").fadeIn();
    $(this.View.$el).fadeIn();
    this.showPage (1);
    setTimeout (function() {
        if (document.activeElement.className == 'uDoc') {
            $(self.View.$el).find("input").first().focus();
        }
    }, 400);
}

// --------------------------------------------------------------------------
// Fade out the dialog and hide the overlay.
// --------------------------------------------------------------------------
Module.Wizard.prototype.hide = function() {
    $("#overlay").fadeOut();
    $("input").blur();
    var v = this;
    if (v.$isView == undefined) v = this.View;
    if (!v || v.$isView == undefined) v = this.view;

    var elm = v.$el;
    $(elm).fadeOut();
}


// --------------------------------------------------------------------------
// TODO: make this generally useful. Will need a second case of a Wizard
//       to pop up in the project to figure out what to do here.
// --------------------------------------------------------------------------
Module.Wizard.prototype.validateAll = function() {
    var v = this;
    if (v.$isView == undefined) v = this.View;
    if (!v || v.$isView == undefined) v = this.view;
    for (k in v) {
        if (typeof (v[k]) == "function") continue;
        if (! this.validate (k, v[k])) return false;
    }
    return true;
}

// --------------------------------------------------------------------------
// Jumps to the previous page.
// --------------------------------------------------------------------------
Module.Wizard.prototype.previousPage = function() {
    var v = this;
    if (v.$isView == undefined) v = this.View;
    if (!v || v.$isView == undefined) v = this.view;

    if (v.page > 1) {
        v.page = v.page-1;
        setTimeout (function() {
            if (document.activeElement.className == 'uDoc') {
                $(v.$el).find("input").first().focus();
            }
        }, 400);
    }
}

// --------------------------------------------------------------------------
// Jumps to the next page. Or calls the finish() method if this was the
// last one.
// --------------------------------------------------------------------------
Module.Wizard.prototype.nextPage = function(e) {
    var v = this;
    if (v.$isView == undefined) v = this.View;
    if (!v || v.$isView == undefined) v = this.view;
    if (! v) {
        console.log ("WTF:",this);
    }

    if (v.pagecount && v.pagecount>1) {
        if (v.page < v.pagecount) {
            v.page = v.page+1;
            setTimeout (function() {
                if (document.activeElement.className == 'uDoc') {
                    $(v.$el).find("input").first().focus();
                }
            }, 400);
        }
        else v.finish();
    }
    else v.finish();
    e.stopPropagation();
}

// --------------------------------------------------------------------------
// Sets up Vue.
// --------------------------------------------------------------------------
Module.Wizard.prototype.createView = function(data) {
    var opt = {
        el: '#'+this.id,
        model: data,
        methods:{},
        watch:{
            page:function(){
                var that = this;
                setTimeout(function(){that.showPage(that.page);},100);
            }
        }
    };

    opt.model.page = 1;
    opt.model.pagecount = this.pagecount;
    
    for (var k in this) {
        if (typeof this[k] == 'function') {
            if (k != 'create') opt.methods[k] = this[k];
        }
    }
    
    this.Vidi = new Vidi.View (this.id, opt);
    this.View = this.Vidi.view;
    console.log ("View created", this);
}
